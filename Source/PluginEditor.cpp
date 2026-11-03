#include "PluginEditor.h"

namespace
{
    constexpr int W = 1000, H = 620;
    // control panel geometry
    constexpr int panelY = 408, contentY = 422;

    juce::Path chevron (bool left)
    {
        juce::Path p;
        if (left) { p.startNewSubPath (15, 6); p.lineTo (9, 12); p.lineTo (15, 18); }
        else      { p.startNewSubPath (9, 6);  p.lineTo (15, 12); p.lineTo (9, 18); }
        return p;
    }

    juce::Path undoIcon (bool redo)
    {
        juce::Path p;
        p.startNewSubPath (9, 14); p.lineTo (4, 9); p.lineTo (9, 4);
        p.startNewSubPath (4, 9); p.lineTo (14, 9);
        p.addCentredArc (14, 15, 6, 6, 0, 0, juce::MathConstants<float>::pi, true);
        p.lineTo (11, 21);
        if (redo)
            p.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (24.0f, 0.0f));
        return p;
    }
}

Core8Content::Core8Content (Core8AudioProcessor& p) : proc (p), display (p)
{
    setOpaque (true);
    auto& s = proc.apvts;

    prevBtn.icon = chevron (true);
    nextBtn.icon = chevron (false);
    undoBtn.icon = undoIcon (false);
    redoBtn.icon = undoIcon (true);
    prevBtn.setTitle ("Previous preset");
    nextBtn.setTitle ("Next preset");
    undoBtn.setTitle ("Undo");
    redoBtn.setTitle ("Redo");

    prevBtn.onClick = [this] { loadPreset ((proc.currentPreset.load() + numFactoryPresets - 1) % numFactoryPresets); };
    nextBtn.onClick = [this] { loadPreset ((proc.currentPreset.load() + 1) % numFactoryPresets); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    abA.onClick = [this] { proc.switchAB (0); updateResetValues(); };
    abB.onClick = [this] { proc.switchAB (1); updateResetValues(); };

    for (auto* c : { &prevBtn, &nextBtn, &presetBtn, &undoBtn, &redoBtn, &abA, &abB })
        addAndMakeVisible (c);

    for (int i = 0; i < Params::fxNames.size(); ++i)
    {
        auto* b = fxBtns.add (new Pill (Params::fxNames[i], Pill::Style::Fx));
        b->dot = Theme::fxDot[i];
        b->onClick = [this, i] { setChoice ("fx", i); };
        addAndMakeVisible (b);
    }
    for (int i = 0; i < Params::beatNames.size(); ++i)
    {
        auto* b = beatBtns.add (new Pill (Params::beatNames[i], Pill::Style::Beat));
        b->onClick = [this, i] { setChoice ("beat", i); };
        addAndMakeVisible (b);
    }

    addAndMakeVisible (display);

    onBtn.isHold = [this] { return proc.getPlain ("hold") > 0.5f; };
    onBtn.isOn = [this] { return proc.getPlain ("fxOn") > 0.5f; };
    onBtn.isLit = [this] { return proc.getPlain ("fxOn") > 0.5f || proc.getPlain ("ride") > 0.001f; };
    onBtn.setOn = [this] (bool v) { proc.setPlain ("fxOn", v ? 1.0f : 0.0f); onBtn.repaint(); };
    onBtn.setTitle ("FX On");
    addAndMakeVisible (onBtn);

    latchBtn.onClick = [this] { proc.undoManager.beginNewTransaction(); proc.setPlain ("hold", 0.0f); };
    holdBtn.onClick = [this] { proc.undoManager.beginNewTransaction(); proc.setPlain ("hold", 1.0f); };
    addAndMakeVisible (latchBtn);
    addAndMakeVisible (holdBtn);

    depthK.attach (s, "depth", &proc.undoManager);
    paramK.attach (s, "param", &proc.undoManager);
    toneK.attach (s, "tone", &proc.undoManager);
    outK.attach (s, "output", &proc.undoManager);
    depthK.setLabel ("Level / Depth");
    toneK.setLabel ("Tone");
    outK.setLabel ("Output");
    toneK.setArc (Theme::textFx);
    outK.setArc (Theme::textFx);
    depthK.format = [] (double v) { return Params::percent ((float) v); };
    paramK.format = [] (double v) { return Params::percent ((float) v); };
    toneK.format = [] (double v) { return Params::toneText ((float) v); };
    outK.format = [] (double v) { return Params::dbText ((float) v); };
    for (auto* k : { &depthK, &paramK, &toneK, &outK })
        addAndMakeVisible (k);

    spillBtn.setClickingTogglesState (true);
    quantBtn.setClickingTogglesState (true);
    spillBtn.onClick = [this] { proc.undoManager.beginNewTransaction(); };
    quantBtn.onClick = [this] { proc.undoManager.beginNewTransaction(); };
    spillAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, "spill", spillBtn);
    quantAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, "quant", quantBtn);
    addAndMakeVisible (spillBtn);
    addAndMakeVisible (quantBtn);

    rideSlider.setSliderStyle (juce::Slider::LinearBar);
    rideSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    rideSlider.setSliderSnapsToMousePosition (false);
    rideSlider.setDoubleClickReturnValue (true, 0.0);
    rideSlider.getProperties().set ("ride", true);
    rideSlider.setTitle ("Ride");
    rideSlider.setTooltip ("One automation lane for the whole transition: above 0 turns the FX on and scales its depth");
    rideSlider.onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
    rideAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, "ride", rideSlider);
    addAndMakeVisible (rideSlider);

    updateResetValues();
    syncFromParams();
    startTimerHz (30);
}

Core8Content::~Core8Content() { stopTimer(); }

void Core8Content::resized()
{
    // top bar
    prevBtn.setBounds (342, 7, 34, 30);
    presetBtn.setBounds (380, 7, 240, 30);
    nextBtn.setBounds (624, 7, 34, 30);
    undoBtn.setBounds (846, 7, 34, 30);
    redoBtn.setBounds (882, 7, 34, 30);
    abA.setBounds (924, 9, 30, 26);
    abB.setBounds (954, 9, 30, 26);

    // effect row
    const float fxW = (968.0f - 7 * 6.0f) / 8.0f;
    for (int i = 0; i < fxBtns.size(); ++i)
        fxBtns[i]->setBounds (juce::roundToInt (16.0f + i * (fxW + 6.0f)), 52, juce::roundToInt (fxW), 36);

    display.setBounds (16, 96, 968, 300);

    // controls
    onBtn.setBounds (16, contentY + 24, 108, 70);
    latchBtn.setBounds (16, contentY + 104, 52, 28);
    holdBtn.setBounds (72, contentY + 104, 52, 28);

    for (int i = 0; i < beatBtns.size(); ++i)
        beatBtns[i]->setBounds (145 + (i % 5) * 43, contentY + 24 + (i / 5) * 39, 38, 34);

    depthK.setBounds (376, contentY, 124, 152);
    paramK.setBounds (521, contentY + 12, 80, 112);
    toneK.setBounds (607, contentY + 12, 80, 112);
    outK.setBounds (693, contentY + 12, 80, 112);

    spillBtn.setBounds (794, contentY + 24, 190, 34);
    quantBtn.setBounds (794, contentY + 66, 190, 34);
    rideSlider.setBounds (794, contentY + 108, 190, 34);
}

void Core8Content::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (Theme::bg);

    // top bar
    g.setColour (Theme::panel);
    g.fillRect (0, 0, W, 44);
    g.setColour (Theme::border);
    g.fillRect (0, 43, W, 1);

    const auto wordFont = Theme::font (24.0f, "Regular", 0.22f);
    g.setFont (wordFont);
    g.setColour (Theme::textBright);
    const float coreW = Theme::textWidth (wordFont, "CORE");
    g.drawText ("CORE", Rectangle<float> (16.0f, 8.0f, coreW + 4.0f, 28.0f), Justification::centredLeft);
    g.setColour (Theme::accentLight);
    g.drawText ("8", Rectangle<float> (16.0f + coreW, 8.0f, 24.0f, 28.0f), Justification::centredLeft);
    g.setColour (Theme::muted);
    g.setFont (Theme::font (12.0f));
    g.drawText ("by zvle", Rectangle<float> (16.0f + coreW + 26.0f, 8.0f, 80.0f, 30.0f), Justification::centredLeft);

    g.setColour (Theme::pillBorder);
    g.drawRoundedRectangle (Rectangle<float> (923.5f, 8.5f, 61.0f, 27.0f), 6.0f, 1.0f);

    // control panel
    g.setColour (Theme::panel2);
    g.fillRect (0, panelY, W, 180);
    g.setColour (Theme::border2);
    g.fillRect (0, panelY, W, 1);
    g.setColour (Theme::border);
    for (int x : { 134, 365, 510, 783 })
        g.fillRect (x, contentY, 1, 152);

    const int fx = (int) proc.getPlain ("fx");
    const int beatIdx = (int) proc.getPlain ("beat");
    auto groupLabel = [&] (const String& t, int x, int w)
    {
        g.setColour (Theme::muted);
        g.setFont (Theme::font (11.0f, "Regular", 0.14f));
        g.drawText (t.toUpperCase(), x, contentY, w, 14, Justification::centredLeft);
    };
    groupLabel ("Effect", 16, 108);
    groupLabel (Params::beatRoleFor (fx), 145, 210);
    groupLabel ("Transition", 794, 190);

    const double bpm = proc.hostBpm.load();
    const double ms = Params::beatValues[jlimit (0, 9, beatIdx)] * 60000.0 / jmax (20.0, bpm);
    const String msText = (ms >= 2000.0 ? String (ms / 1000.0, 2) + " s" : String (roundToInt (ms)) + " ms")
                          + " @ " + String (bpm, 0) + " BPM";
    g.setColour (Theme::textPill);
    g.setFont (Theme::font (14.0f));
    g.drawText (msText, 145, contentY + 107, 210, 18, Justification::centredLeft);

    // status bar
    g.setColour (Theme::statusBg);
    g.fillRect (0, 588, W, 32);
    g.setColour (Colour (0xff222830));
    g.fillRect (0, 588, W, 1);
    const bool playing = proc.hostPlaying.load();
    g.setColour (playing ? Theme::green : Theme::knobRim);
    g.fillEllipse (16.0f, 600.5f, 7.0f, 7.0f);
    g.setColour (Theme::muted);
    g.setFont (Theme::font (12.0f));
    g.drawText ((playing ? "Synced to host" : "Host stopped") + String::fromUTF8 ("  \xc2\xb7  ")
                    + String (bpm, 2) + " BPM",
                31, 588, 400, 32, Justification::centredLeft);
    g.drawText (String::fromUTF8 ("Latency 0 smp     CORE8 v0.1"), 600, 588, 384, 32, Justification::centredRight);
}

void Core8Content::setChoice (const juce::String& id, int index)
{
    proc.undoManager.beginNewTransaction();
    proc.setPlain (id, (float) index);
    syncFromParams();
}

void Core8Content::loadPreset (int index)
{
    proc.loadPreset (index);
    updateResetValues();
    syncFromParams();
}

void Core8Content::updateResetValues()
{
    const auto& pr = factoryPresets[juce::jlimit (0, numFactoryPresets - 1, proc.currentPreset.load())];
    depthK.slider.setDoubleClickReturnValue (true, pr.depth);
    paramK.slider.setDoubleClickReturnValue (true, pr.param);
    toneK.slider.setDoubleClickReturnValue (true, pr.tone);
    outK.slider.setDoubleClickReturnValue (true, pr.outDb);
}

void Core8Content::showPresetMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("FACTORY PRESETS");
    for (int i = 0; i < numFactoryPresets; ++i)
        m.addItem (i + 1, juce::String (factoryPresets[i].name) + "     " + Params::fxNames[factoryPresets[i].fx]
                              + " " + Params::beatNames[factoryPresets[i].beat],
                   true, i == proc.currentPreset.load());

    juce::Component::SafePointer<Core8Content> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn).withMinimumWidth (240),
                     [safe] (int r) { if (safe != nullptr && r > 0) safe->loadPreset (r - 1); });
}

void Core8Content::syncFromParams()
{
    const int fx = (int) proc.getPlain ("fx");
    const int beat = (int) proc.getPlain ("beat");
    const bool hold = proc.getPlain ("hold") > 0.5f;

    for (int i = 0; i < fxBtns.size(); ++i)
        fxBtns[i]->setToggleState (i == fx, juce::dontSendNotification);
    for (int i = 0; i < beatBtns.size(); ++i)
        beatBtns[i]->setToggleState (i == beat, juce::dontSendNotification);
    latchBtn.setToggleState (! hold, juce::dontSendNotification);
    holdBtn.setToggleState (hold, juce::dontSendNotification);
    abA.setToggleState (proc.getABSlot() == 0, juce::dontSendNotification);
    abB.setToggleState (proc.getABSlot() == 1, juce::dontSendNotification);

    paramK.setLabel (Params::paramNameFor (fx));

    const juce::String name = juce::String (factoryPresets[juce::jlimit (0, numFactoryPresets - 1, proc.currentPreset.load())].name)
                              + (proc.isPresetDirty() ? " *" : "");
    if (presetBtn.getButtonText() != name)
        presetBtn.setButtonText (name);

    if (fx != lastFx || beat != lastBeat)
    {
        lastFx = fx;
        lastBeat = beat;
        repaint (0, panelY, W, 180);
    }
}

void Core8Content::timerCallback()
{
    syncFromParams();
    display.refresh();

    const bool lit = onBtn.isLit();
    if (lit != lastLit)
    {
        lastLit = lit;
        onBtn.repaint();
    }

    const juce::String status = juce::String (proc.hostBpm.load(), 2) + (proc.hostPlaying.load() ? "p" : "s");
    if (status != lastStatus)
    {
        lastStatus = status;
        repaint (0, 588, W, 32);
        repaint (145, contentY + 107, 210, 18);
    }
}

//==============================================================================
Core8AudioProcessorEditor::Core8AudioProcessorEditor (Core8AudioProcessor& p)
    : AudioProcessorEditor (&p), content (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (600, 372, 1600, 992);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) W / (double) H);
    setSize (W, H);
}

Core8AudioProcessorEditor::~Core8AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void Core8AudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) W;
    content.setBounds (0, 0, W, H);
    content.setTransform (juce::AffineTransform::scale (scale));
}
