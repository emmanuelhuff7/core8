#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/FxEngine.h"
#include "Parameters.h"
#include "Presets.h"

class Core8AudioProcessor final : public juce::AudioProcessor
{
public:
    Core8AudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CORE8"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return numFactoryPresets; }
    int getCurrentProgram() override { return currentPreset.load(); }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadPreset (int index);
    bool isPresetDirty() const;
    void switchAB (int slot);
    int getABSlot() const noexcept { return abSlot; }

    float getPlain (const juce::String& id) const;
    void setPlain (const juce::String& id, float plainValue);

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f }, hostBpm { 124.0f };
    std::atomic<bool> fxActive { false }, hostPlaying { false };
    std::atomic<int> currentPreset { 1 };

private:
    FxEngine engine;
    std::atomic<float>* pFx = nullptr;
    std::atomic<float>* pBeat = nullptr;
    std::atomic<float>* pDepth = nullptr;
    std::atomic<float>* pParam = nullptr;
    std::atomic<float>* pTone = nullptr;
    std::atomic<float>* pOut = nullptr;
    std::atomic<float>* pOn = nullptr;
    std::atomic<float>* pRide = nullptr;
    std::atomic<float>* pSpill = nullptr;
    std::atomic<float>* pQuant = nullptr;

    float depthScale = 1.0f;   // last level that drove the FX, so spillover tails keep it

    juce::ValueTree abSlots[2];
    int abSlot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Core8AudioProcessor)
};
