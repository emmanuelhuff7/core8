#pragma once

#include "PluginProcessor.h"
#include "UI/Widgets.h"
#include "UI/FxDisplay.h"

// Fixed 1000 x 620 layout; the editor scales it when the window is resized.
class Core8Content : public juce::Component, private juce::Timer
{
public:
    explicit Core8Content (Core8AudioProcessor&);
    ~Core8Content() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void syncFromParams();
    void setChoice (const juce::String& id, int index);
    void showPresetMenu();
    void loadPreset (int index);
    void updateResetValues();

    Core8AudioProcessor& proc;

    Pill prevBtn { "prev", Pill::Style::Icon }, nextBtn { "next", Pill::Style::Icon };
    Pill presetBtn { "Echo Out", Pill::Style::Preset };
    Pill undoBtn { "undo", Pill::Style::Icon }, redoBtn { "redo", Pill::Style::Icon };
    Pill abA { "A", Pill::Style::Ab }, abB { "B", Pill::Style::Ab };

    juce::OwnedArray<Pill> fxBtns, beatBtns;
    FxDisplay display;

    OnButton onBtn;
    Pill latchBtn { "LATCH", Pill::Style::Small }, holdBtn { "HOLD", Pill::Style::Small };
    KnobBlock depthK { true, false }, paramK { false, false }, toneK { false, true }, outK { false, true };
    Pill spillBtn { "Spillover", Pill::Style::Toggle }, quantBtn { "Quantize", Pill::Style::Toggle };
    juce::Slider rideSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rideAtt;
    bool lastLit = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> spillAtt, quantAtt;

    int lastFx = -1, lastBeat = -1;
    juce::String lastStatus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Core8Content)
};

class Core8AudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit Core8AudioProcessorEditor (Core8AudioProcessor&);
    ~Core8AudioProcessorEditor() override;
    void resized() override;

private:
    CoreLookAndFeel lnf;
    Core8Content content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Core8AudioProcessorEditor)
};
