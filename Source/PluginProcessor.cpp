#include "PluginProcessor.h"
#include "PluginEditor.h"

Core8AudioProcessor::Core8AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "CORE8", Params::createLayout())
{
    pFx    = apvts.getRawParameterValue ("fx");
    pBeat  = apvts.getRawParameterValue ("beat");
    pDepth = apvts.getRawParameterValue ("depth");
    pParam = apvts.getRawParameterValue ("param");
    pTone  = apvts.getRawParameterValue ("tone");
    pOut   = apvts.getRawParameterValue ("output");
    pOn    = apvts.getRawParameterValue ("fxOn");
    pRide  = apvts.getRawParameterValue ("ride");
    pSpill = apvts.getRawParameterValue ("spill");
    pQuant = apvts.getRawParameterValue ("quant");
}

void Core8AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
}

bool Core8AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void Core8AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    HostInfo host;
    host.bpm = hostBpm.load();
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) host.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) { host.ppq = *ppq; host.ppqValid = true; }
            host.playing = pos->getIsPlaying();
        }
    }
    hostBpm.store ((float) host.bpm);
    hostPlaying.store (host.playing);

    FxParams p;
    p.fx = juce::jlimit (0, NumFx - 1, (int) pFx->load());
    p.beats = Params::beatValues[juce::jlimit (0, 9, (int) pBeat->load())];
    // FX On wins; otherwise Ride above zero switches the effect on and scales its depth.
    const bool onSwitch = pOn->load() > 0.5f;
    const float ride = pRide->load();
    const bool targetOn = onSwitch || ride > 0.001f;
    if (onSwitch)            depthScale = 1.0f;
    else if (ride > 0.001f)  depthScale = ride;   // when both are off, tails keep the last level
    p.depth = pDepth->load() * depthScale;
    p.param = pParam->load();
    p.tone = pTone->load();
    p.outputDb = pOut->load();
    p.spill = pSpill->load() > 0.5f;

    const float decay = std::pow (0.5f, (float) n / (0.08f * (float) getSampleRate()));
    inLevel.store (juce::jmax (buffer.getMagnitude (0, n), inLevel.load() * decay));

    engine.process (buffer, p, host, targetOn, pQuant->load() > 0.5f);

    outLevel.store (juce::jmax (buffer.getMagnitude (0, n), outLevel.load() * decay));
    fxActive.store (engine.isActive());
}

const juce::String Core8AudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, numFactoryPresets) ? factoryPresets[index].name : "";
}

float Core8AudioProcessor::getPlain (const juce::String& id) const
{
    if (auto* p = apvts.getParameter (id))
        return p->convertFrom0to1 (p->getValue());
    return 0.0f;
}

void Core8AudioProcessor::setPlain (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

void Core8AudioProcessor::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, numFactoryPresets))
        return;

    const auto& pr = factoryPresets[index];
    undoManager.beginNewTransaction ("Load preset");
    setPlain ("fx", (float) pr.fx);
    setPlain ("beat", (float) pr.beat);
    setPlain ("depth", pr.depth);
    setPlain ("param", pr.param);
    setPlain ("tone", pr.tone);
    setPlain ("output", pr.outDb);
    setPlain ("hold", pr.hold ? 1.0f : 0.0f);
    setPlain ("spill", pr.spill ? 1.0f : 0.0f);
    setPlain ("quant", pr.quant ? 1.0f : 0.0f);
    currentPreset.store (index);
}

bool Core8AudioProcessor::isPresetDirty() const
{
    const auto& pr = factoryPresets[juce::jlimit (0, numFactoryPresets - 1, currentPreset.load())];
    auto differs = [this] (const char* id, float v, float tol) { return std::abs (getPlain (id) - v) > tol; };
    return differs ("fx", (float) pr.fx, 0.5f) || differs ("beat", (float) pr.beat, 0.5f)
        || differs ("depth", pr.depth, 0.005f) || differs ("param", pr.param, 0.005f)
        || differs ("tone", pr.tone, 0.005f) || differs ("output", pr.outDb, 0.05f)
        || differs ("hold", pr.hold ? 1.0f : 0.0f, 0.5f) || differs ("spill", pr.spill ? 1.0f : 0.0f, 0.5f)
        || differs ("quant", pr.quant ? 1.0f : 0.0f, 0.5f);
}

void Core8AudioProcessor::switchAB (int slot)
{
    if (slot == abSlot || ! juce::isPositiveAndBelow (slot, 2))
        return;

    abSlots[abSlot] = apvts.copyState();
    abSlots[abSlot].setProperty ("preset", currentPreset.load(), nullptr);

    if (abSlots[slot].isValid())
    {
        currentPreset.store ((int) abSlots[slot].getProperty ("preset", currentPreset.load()));
        apvts.replaceState (abSlots[slot].createCopy());
    }
    abSlot = slot;
}

void Core8AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void Core8AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentPreset.store ((int) tree.getProperty ("preset", 1));
            apvts.replaceState (tree);
        }
    }
}

juce::AudioProcessorEditor* Core8AudioProcessor::createEditor()
{
    return new Core8AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Core8AudioProcessor();
}
