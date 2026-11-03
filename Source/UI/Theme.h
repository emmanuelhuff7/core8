#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Theme
{
    inline const juce::Colour bg          { 0xff161a20 };
    inline const juce::Colour panel       { 0xff1d2229 };
    inline const juce::Colour panel2      { 0xff1b1f26 };
    inline const juce::Colour display     { 0xff0e1116 };
    inline const juce::Colour border      { 0xff2a313b };
    inline const juce::Colour border2     { 0xff262d36 };
    inline const juce::Colour pillBg      { 0xff20252d };
    inline const juce::Colour pillBorder  { 0xff2f3742 };
    inline const juce::Colour text        { 0xffd5dce5 };
    inline const juce::Colour textBright  { 0xfff2f5f8 };
    inline const juce::Colour textValue   { 0xffe6ebf1 };
    inline const juce::Colour textPill    { 0xffc3ccd6 };
    inline const juce::Colour textFx      { 0xffb9c3cf };
    inline const juce::Colour muted       { 0xff8b96a5 };
    inline const juce::Colour subtle      { 0xffaab4c0 };
    inline const juce::Colour accent      { 0xff4d4775 };
    inline const juce::Colour accentEdge  { 0xff6a63a0 };
    inline const juce::Colour accentLight { 0xffa29bd9 };
    inline const juce::Colour knobBody    { 0xff262c35 };
    inline const juce::Colour knobInner   { 0xff2c333d };
    inline const juce::Colour knobRim     { 0xff3a424d };
    inline const juce::Colour knobTrack   { 0xff2b323c };
    inline const juce::Colour pointer     { 0xffeef2f6 };
    inline const juce::Colour gridMinor   { 0xff1f252d };
    inline const juce::Colour gridMajor   { 0xff2d3540 };
    inline const juce::Colour meterTrack  { 0xff1b2028 };
    inline const juce::Colour meter       { 0xff7f8b9a };
    inline const juce::Colour statusBg    { 0xff12151a };
    inline const juce::Colour green       { 0xff3fdc8a };

    // Per-effect identification dots (Echo .. Roll).
    inline const juce::Colour fxDot[] { juce::Colour (0xff3fc7e8), juce::Colour (0xff5b9dff), juce::Colour (0xff9a86ff),
                                        juce::Colour (0xffd07cff), juce::Colour (0xffff6fae), juce::Colour (0xffff9a3c),
                                        juce::Colour (0xffffd23f), juce::Colour (0xff3fdc8a) };

    // Helvetica Neue ships with macOS. Fall back to the system sans-serif if it is missing,
    // and to the Regular face if a weight is missing, so text never fails to resolve.
    inline const juce::String& family()
    {
        static const juce::String f = juce::Font::findAllTypefaceNames().contains ("Helvetica Neue")
                                          ? juce::String ("Helvetica Neue")
                                          : juce::Font::getDefaultSansSerifFontName();
        return f;
    }

    inline juce::String styleFor (const juce::String& wanted)
    {
        static const juce::StringArray styles = juce::Font::findAllTypefaceStyles (family());
        if (styles.contains (wanted)) return wanted;
        if (styles.contains ("Regular")) return "Regular";
        return styles.isEmpty() ? juce::String ("Regular") : styles[0];
    }

    // Sizes are CSS-style pixel sizes.
    inline juce::Font font (float px, const char* style = "Regular", float kerning = 0.0f)
    {
        if (family() != "Helvetica Neue")
            return juce::Font (juce::FontOptions (px).withPointHeight (px)).withExtraKerningFactor (kerning);

        return juce::Font (juce::FontOptions (family(), styleFor (style), px).withPointHeight (px))
                   .withExtraKerningFactor (kerning);
    }

    inline float textWidth (const juce::Font& f, const juce::String& s)
    {
        return juce::GlyphArrangement::getStringWidth (f, s);
    }
}
