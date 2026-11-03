#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

//==============================================================================
class CoreLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CoreLookAndFeel()
    {
        if (Theme::family() == "Helvetica Neue")
            setDefaultSansSerifTypefaceName ("Helvetica Neue");
        setColour (juce::PopupMenu::backgroundColourId, Theme::panel);
        setColour (juce::PopupMenu::textColourId, Theme::text);
        setColour (juce::PopupMenu::headerTextColourId, Theme::muted);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::accent);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }

    juce::Font getPopupMenuFont() override { return Theme::font (15.0f); }

    // The Ride bar: a filled horizontal fader with its label and value inside.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float minPos,
                           float maxPos, juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        using namespace juce;
        if (! (bool) s.getProperties()["ride"])
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
            return;
        }
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (0.5f);
        Path shape;
        shape.addRoundedRectangle (b, 6.0f);
        g.setColour (Theme::pillBg);
        g.fillPath (shape);
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (shape);
            g.setColour (Theme::accent);
            g.fillRect (b.withRight (jlimit (b.getX(), b.getRight(), sliderPos)));
        }
        g.setColour (s.getValue() > 0.001 ? Theme::accentEdge : Theme::pillBorder);
        g.strokePath (shape, PathStrokeType (1.0f));
        g.setColour (Theme::text);
        g.setFont (Theme::font (14.0f));
        g.drawText ("Ride", b.reduced (10.0f, 0.0f), Justification::centredLeft);
        g.setColour (s.getValue() > 0.001 ? Colours::white : Theme::muted);
        g.drawText (String (roundToInt (s.getValue() * 100.0)) + "%", b.reduced (10.0f, 0.0f), Justification::centredRight);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        const bool big = s.getProperties()["big"];
        const bool bipolar = s.getProperties()["bipolar"];
        const auto arcColour = juce::Colour::fromString (s.getProperties()["arc"].toString());

        const float size = (float) juce::jmin (w, h);
        const float sc = size / 100.0f;
        const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
        const float r = 42.0f * sc;
        const float stroke = (big ? 5.0f : 6.0f) * sc;
        const float ang = a0 + pos * (a1 - a0);
        const juce::PathStrokeType pst (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track;
        track.addCentredArc (cx, cy, r, r, 0.0f, a0, a1, true);
        g.setColour (Theme::knobTrack);
        g.strokePath (track, pst);

        const float from = bipolar ? (a0 + a1) * 0.5f : a0;
        if (std::abs (ang - from) > 0.01f)
        {
            juce::Path val;
            val.addCentredArc (cx, cy, r, r, 0.0f, juce::jmin (from, ang), juce::jmax (from, ang), true);
            g.setColour (arcColour);
            g.strokePath (val, pst);
        }

        const float body = (big ? 33.0f : 31.0f) * sc;
        g.setColour (Theme::knobBody);
        g.fillEllipse (cx - body, cy - body, body * 2, body * 2);
        g.setColour (Theme::knobRim);
        g.drawEllipse (cx - body, cy - body, body * 2, body * 2, 1.5f * sc);
        if (big)
        {
            const float inner = 27.0f * sc;
            g.setColour (Theme::knobInner);
            g.fillEllipse (cx - inner, cy - inner, inner * 2, inner * 2);
        }

        auto pt = [&] (float rr) { return juce::Point<float> (cx + rr * std::sin (ang), cy - rr * std::cos (ang)); };
        juce::Path ptr;
        ptr.startNewSubPath (pt (10.0f * sc));
        ptr.lineTo (pt (26.0f * sc));
        g.setColour (Theme::pointer);
        g.strokePath (ptr, juce::PathStrokeType ((big ? 3.0f : 4.0f) * sc, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }
};

//==============================================================================
class Pill : public juce::Button
{
public:
    enum class Style { Fx, Beat, Small, Toggle, Preset, Icon, Ab };

    Pill (const juce::String& name, Style s) : juce::Button (name), style (s)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    juce::Colour dot;
    juce::Path icon;   // in a 24 x 24 box

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        using namespace juce;
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = getToggleState();
        const String label = getButtonText();

        auto box = [&] (Colour fill, Colour edge, float radius)
        {
            g.setColour (fill);
            g.fillRoundedRectangle (b, radius);
            g.setColour (edge);
            g.drawRoundedRectangle (b, radius, 1.0f);
        };

        switch (style)
        {
            case Style::Fx:
            {
                box (on ? Theme::accent : Theme::panel, on ? Theme::accentEdge : Theme::border, 6.0f);
                const auto f = Theme::font (15.0f, on ? "Medium" : "Regular", 0.02f);
                const float tw = Theme::textWidth (f, label);
                float x0 = b.getCentreX() - (8.0f + 8.0f + tw) * 0.5f;
                g.setColour (dot.withAlpha (on ? 1.0f : 0.55f));
                g.fillEllipse (x0, b.getCentreY() - 4.0f, 8.0f, 8.0f);
                g.setColour (on ? Colours::white : Theme::textFx);
                g.setFont (f);
                g.drawText (label, Rectangle<float> (x0 + 16.0f, b.getY(), tw + 4.0f, b.getHeight()), Justification::centredLeft);
                break;
            }
            case Style::Beat:
                box (on ? Theme::accent : Theme::pillBg, on ? Theme::accentEdge : Theme::pillBorder, 6.0f);
                g.setColour (on ? Colours::white : Theme::textPill);
                g.setFont (Theme::font (13.0f, on ? "Medium" : "Regular"));
                g.drawText (label, b, Justification::centred);
                break;

            case Style::Small:
                box (on ? Theme::accent : Colours::transparentBlack, on ? Theme::accentEdge : Theme::pillBorder, 5.0f);
                g.setColour (on ? Colours::white : Theme::subtle);
                g.setFont (Theme::font (11.0f, "Regular", 0.08f));
                g.drawText (label, b, Justification::centred);
                break;

            case Style::Toggle:
            {
                box (Theme::pillBg, on ? Colour (0xff3a4350) : Theme::pillBorder, 6.0f);
                g.setColour (Theme::text);
                g.setFont (Theme::font (14.0f));
                g.drawText (label, b.reduced (10.0f, 0.0f), Justification::centredLeft);
                g.setColour (on ? Theme::accentLight : Theme::knobRim);
                g.fillEllipse (b.getRight() - 18.0f, b.getCentreY() - 4.0f, 8.0f, 8.0f);
                break;
            }
            case Style::Preset:
            {
                box (Theme::bg, Theme::pillBorder, 6.0f);
                const auto f = Theme::font (16.0f, "Regular", 0.04f);
                const float tw = Theme::textWidth (f, label);
                const float x0 = b.getCentreX() - (tw + 20.0f) * 0.5f;
                g.setColour (Theme::textValue);
                g.setFont (f);
                g.drawText (label, Rectangle<float> (x0, b.getY(), tw + 4.0f, b.getHeight()), Justification::centredLeft);
                Path chev;
                const float cxv = x0 + tw + 14.0f, cyv = b.getCentreY();
                chev.startNewSubPath (cxv - 4.0f, cyv - 2.0f);
                chev.lineTo (cxv, cyv + 2.0f);
                chev.lineTo (cxv + 4.0f, cyv - 2.0f);
                g.setColour (Theme::muted);
                g.strokePath (chev, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
                break;
            }
            case Style::Icon:
            {
                if (over || down)
                {
                    g.setColour (Colours::white.withAlpha (down ? 0.10f : 0.05f));
                    g.fillRoundedRectangle (b, 6.0f);
                }
                auto area = b.withSizeKeepingCentre (16.0f, 16.0f);
                auto p = icon;
                p.applyTransform (AffineTransform::scale (16.0f / 24.0f).translated (area.getX(), area.getY()));
                g.setColour (Theme::subtle);
                g.strokePath (p, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
                break;
            }
            case Style::Ab:
                g.setColour (on ? Colour (0xff3a424d) : Colours::transparentBlack);
                g.fillRect (getLocalBounds());
                g.setColour (on ? Theme::textBright : Theme::muted);
                g.setFont (Theme::font (13.0f));
                g.drawText (label, getLocalBounds(), Justification::centred);
                break;
        }

        if (over && style != Style::Icon && style != Style::Ab && ! on)
        {
            g.setColour (Colours::white.withAlpha (0.03f));
            g.fillRoundedRectangle (b, 6.0f);
        }
    }

private:
    Style style;
};

//==============================================================================
class KnobBlock : public juce::Component, private juce::Slider::Listener
{
public:
    KnobBlock (bool isBig, bool bipolar) : big (isBig)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (180);
        slider.setVelocityModeParameters (0.25, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        slider.getProperties().set ("big", big);
        slider.getProperties().set ("bipolar", bipolar);
        slider.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        slider.addListener (this);
        addAndMakeVisible (slider);
        setArc (Theme::accentLight);
    }

    ~KnobBlock() override { slider.removeListener (this); }

    void attach (juce::AudioProcessorValueTreeState& s, const juce::String& id, juce::UndoManager* um)
    {
        undo = um;
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
        slider.setTitle (id);
    }

    void setArc (juce::Colour c)
    {
        if (c.toString() != slider.getProperties()["arc"].toString())
        {
            slider.getProperties().set ("arc", c.toString());
            slider.repaint();
        }
    }

    void setLabel (const juce::String& l)
    {
        if (l != label) { label = l; repaint(); }
    }

    std::function<juce::String (double)> format;
    juce::Slider slider;

    void resized() override
    {
        const int k = big ? 100 : 68;
        slider.setBounds ((getWidth() - k) / 2, 0, k, k);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const int top = big ? 102 : 72;
        g.setColour (Theme::muted);
        g.setFont (Theme::font (11.0f, "Regular", big ? 0.14f : 0.12f));
        g.drawText (label.toUpperCase(), 0, top, getWidth(), 14, Justification::centred);

        const String v = format ? format (slider.getValue()) : String (slider.getValue());
        if (big)
        {
            g.setColour (Theme::accentLight);
            g.setFont (Theme::font (22.0f));
            g.drawText (v, 0, top + 16, getWidth(), 26, Justification::centred);
        }
        else
        {
            g.setColour (Theme::textValue);
            g.setFont (Theme::font (16.0f));
            g.drawText (v, 0, top + 16, getWidth(), 20, Justification::centred);
        }
    }

private:
    void sliderValueChanged (juce::Slider*) override { repaint(); }
    void sliderDragStarted (juce::Slider*) override { if (undo != nullptr) undo->beginNewTransaction(); }

    bool big;
    juce::String label;
    juce::UndoManager* undo = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
// FX ON: click to toggle in Latch mode, press-and-hold in Hold mode.
class OnButton : public juce::Component
{
public:
    std::function<bool()> isHold, isOn, isLit;   // lit = FX On or Ride above zero
    std::function<void (bool)> setOn;

    OnButton() { setMouseCursor (juce::MouseCursor::PointingHandCursor); setWantsKeyboardFocus (true); }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (isHold && isHold()) setOn (true);
        else if (isOn) setOn (! isOn());
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (isHold && isHold()) setOn (false);
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::spaceKey || k == juce::KeyPress::returnKey)
        {
            if (isOn) setOn (! isOn());
            return true;
        }
        return false;
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const bool on = isLit ? isLit() : (isOn && isOn());
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        if (on)
        {
            Path glow;
            glow.addRoundedRectangle (b, 8.0f);
            DropShadow (Theme::accent.withAlpha (0.55f), 18, {}).drawForPath (g, glow);
        }
        g.setColour (on ? Theme::accent : Theme::knobBody);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (on ? Theme::accentEdge : Theme::knobRim);
        g.drawRoundedRectangle (b, 8.0f, 1.0f);
        g.setColour (on ? Colours::white : Theme::textPill);
        g.setFont (Theme::font (20.0f, on ? "Medium" : "Regular", 0.1f));
        g.drawText (on ? "FX ON" : "FX OFF", getLocalBounds(), Justification::centred);
    }
};
