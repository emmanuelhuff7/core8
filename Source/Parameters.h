#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Params
{
    inline const juce::StringArray fxNames { "Echo", "Delay", "Ping Pong", "Reverb",
                                             "Flanger", "Phaser", "Filter", "Roll" };

    inline const juce::StringArray beatNames { "1/16", "1/8", "1/4", "1/2", "3/4",
                                               "1", "2", "4", "8", "16" };

    inline constexpr double beatValues[] { 0.0625, 0.125, 0.25, 0.5, 0.75, 1.0, 2.0, 4.0, 8.0, 16.0 };

    inline juce::String paramNameFor (int fx)
    {
        switch (fx)
        {
            case 3:  return "Decay";
            case 5:  return "Resonance";
            case 6:  return "Resonance";
            case 7:  return "Gate";
            default: return "Feedback";
        }
    }

    inline juce::String beatRoleFor (int fx)
    {
        switch (fx)
        {
            case 0:  return "Echo time";
            case 1:  return "Delay time";
            case 2:  return "Ping time";
            case 3:  return "Pre-delay";
            case 7:  return "Slice length";
            default: return "Sweep cycle";
        }
    }

    inline juce::String percent (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

    inline juce::String toneText (float t)
    {
        const float v = (t - 0.5f) * 2.0f;
        if (std::abs (v) < 0.03f) return "Flat";
        return (v < 0.0f ? "LPF " : "HPF ") + juce::String (juce::roundToInt (std::abs (v) * 100.0f)) + "%";
    }

    inline juce::String dbText (float db)
    {
        return (db >= 0.0f ? "+" : "") + juce::String (db, 1) + " dB";
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;

        auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return percent (v); });

        // The two transition controls come first so they top the host's automation list.
        // FX On: hard switch. Ride: one lane that switches the FX on above 0 and scales its depth,
        // so a whole transition is a single drawn ramp.
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "fxOn", 1 }, "FX On", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "ride", 1 }, "Ride",
                                                           NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));

        // Defaults = the "Echo Out" preset.
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "fx", 1 }, "Effect", fxNames, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "beat", 1 }, "Beat", beatNames, 3));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "depth", 1 }, "Level/Depth",
                                                           NormalisableRange<float> (0.0f, 1.0f), 0.62f, pct));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "param", 1 }, "FX Param",
                                                           NormalisableRange<float> (0.0f, 1.0f), 0.72f, pct));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "tone", 1 }, "Tone",
                                                           NormalisableRange<float> (0.0f, 1.0f), 0.65f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (
                                                               [] (float v, int) { return toneText (v); })));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                                                           NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (
                                                               [] (float v, int) { return dbText (v); })));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "hold", 1 }, "Hold Mode", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "spill", 1 }, "Spillover", true));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "quant", 1 }, "Quantize", true));
        return layout;
    }
}
