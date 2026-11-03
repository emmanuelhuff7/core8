#include "FxDisplay.h"
#include "../PluginProcessor.h"

namespace
{
    constexpr float X0 = 40.0f, X1 = 898.0f, TOP = 86.0f, BASE = 262.0f, MID = 174.0f;
    constexpr float H = BASE - TOP;

    float xb (double b) { return X0 + (float) (b / 8.0) * (X1 - X0); }
    float xf (double f) { return X0 + (float) (std::log10 (f / 20.0) / 3.0) * (X1 - X0); }
    float ydb (double db) { return 158.0f - (float) juce::jlimit (-25.0, 18.0, db) * 4.0f; }

    void bar (juce::Path& p, float x, float w, float yTop, float yBot)
    {
        p.addRectangle (x, yTop, w, yBot - yTop);
    }

    juce::String freqLabel (int f) { return f >= 1000 ? juce::String (f / 1000) + "k" : juce::String (f); }
}

FxDisplay::FxDisplay (Core8AudioProcessor& p) : proc (p), meters (p)
{
    setInterceptsMouseClicks (false, false);
    setBufferedToImage (true);   // the static layer is cached until refresh() sees a change
    addAndMakeVisible (meters);
    shown = readState();
}

void FxDisplay::resized()
{
    // meter area in display coordinates (+1 for the border)
    meters.setBounds (1 + 912, 1 + (int) TOP, 36, (int) H);
}

FxDisplay::State FxDisplay::readState() const
{
    State s;
    s.fx = juce::jlimit (0, 7, (int) proc.getPlain ("fx"));
    s.beat = juce::jlimit (0, 9, (int) proc.getPlain ("beat"));
    s.depthQ = juce::roundToInt (proc.getPlain ("depth") * 1000.0f);
    s.paramQ = juce::roundToInt (proc.getPlain ("param") * 1000.0f);
    s.bpmQ = juce::roundToInt (proc.hostBpm.load() * 100.0f);
    s.on = proc.fxActive.load();
    return s;
}

void FxDisplay::refresh()
{
    const auto s = readState();
    if (s != shown)
    {
        shown = s;
        repaint();
    }
    meters.refresh();
}

void FxDisplay::Meters::refresh()
{
    auto meterH = [this] (float level)
    {
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        return juce::roundToInt (juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f) * (float) getHeight());
    };
    const int i = meterH (proc.inLevel.load()), o = meterH (proc.outLevel.load());
    const bool a = proc.fxActive.load();
    if (i != inH || o != outH || a != on)
    {
        inH = i; outH = o; on = a;
        repaint();
    }
}

void FxDisplay::Meters::paint (juce::Graphics& g)
{
    g.fillAll (Theme::display);
    const int h = getHeight();
    g.setColour (Theme::meterTrack);
    g.fillRect (0, 0, 10, h);
    g.fillRect (26, 0, 10, h);
    g.setColour (Theme::meter);
    g.fillRect (0, h - juce::jmax (0, inH), 10, juce::jmax (0, inH));
    g.setColour (Theme::accentLight.withAlpha (on ? 1.0f : 0.6f));
    g.fillRect (26, h - juce::jmax (0, outH), 10, juce::jmax (0, outH));
}

void FxDisplay::paint (juce::Graphics& g)
{
    using namespace juce;

    const int fx = shown.fx;
    const int beatIdx = shown.beat;
    const double beat = Params::beatValues[beatIdx];
    const float depth = (float) shown.depthQ / 1000.0f;
    const float param = (float) shown.paramQ / 1000.0f;
    const bool on = shown.on;
    const double bpm = (double) shown.bpmQ / 100.0;
    const double msPerBeat = 60000.0 / jmax (20.0, bpm);

    auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::display);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (Theme::border2);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (bounds.reduced (1.0f).toNearestInt());
    g.addTransform (AffineTransform::translation (1.0f, 1.0f));

    Path gridMinor, gridMajor, wetFill, wetStroke, dry;
    struct Label { String text; float x; };
    std::vector<Label> labels;
    const bool isTime = ! (fx == 4 || fx == 5 || fx == 6);

    if (isTime)
    {
        for (int k = 0; k <= 16; ++k)
        {
            const double b = k * 0.5;
            auto& gp = (k % 8 == 0) ? gridMajor : gridMinor;
            gp.startNewSubPath (xb (b), TOP - 10.0f);
            gp.lineTo (xb (b), BASE);
        }
        gridMajor.startNewSubPath (X0, BASE);
        gridMajor.lineTo (X1, BASE);
        for (int b = 0; b < 8; ++b)
            labels.push_back ({ String (b / 4 + 1) + "." + String (b % 4 + 1), xb (b) });

        bar (dry, xb (0) + 0.5f, 3.0f, BASE - H * 0.95f, BASE);

        if (fx == 0 || fx == 1)
        {
            const double fb = fx == 0 ? 0.3 + 0.62 * param : 0.05 + 0.35 * param;
            bool first = true;
            for (int k = 1; k * beat <= 8.0; ++k)
            {
                const double amp = depth * std::pow (fb, k - 1);
                if (amp < 0.012) break;
                const float x = xb (k * beat);
                const float h = H * (float) amp * 0.92f;
                bar (wetFill, x - 2.5f, 5.0f, BASE - h, BASE);
                if (fx == 0)
                {
                    if (first) wetStroke.startNewSubPath (x, BASE - h);
                    else wetStroke.lineTo (x, BASE - h);
                    first = false;
                }
            }
        }
        else if (fx == 2)
        {
            const double fb = 0.25 + 0.6 * param;
            gridMajor.startNewSubPath (X0, MID);
            gridMajor.lineTo (X1, MID);
            dry.clear();
            bar (dry, xb (0), 3.0f, MID - (MID - TOP) * 0.9f, MID + (MID - TOP) * 0.9f);
            for (int k = 1; k * beat <= 8.0; ++k)
            {
                const double amp = depth * std::pow (fb, k - 1);
                if (amp < 0.012) break;
                const float x = xb (k * beat);
                const float h = (MID - TOP) * 0.92f * (float) amp;
                if (k % 2) bar (wetFill, x - 2.5f, 5.0f, MID - h, MID);
                else       bar (wetFill, x - 2.5f, 5.0f, MID, MID + h);
            }
        }
        else if (fx == 3)
        {
            const double pre = jmin (beat, 2.0), T = 0.5 + param * 5.0;
            wetStroke.startNewSubPath (xb (pre), BASE);
            for (int i = 0; i <= 160; ++i)
            {
                const double b = pre + (8.0 - pre) * i / 160.0;
                const double env = depth * 0.85 * jmin (1.0, (b - pre) / 0.06) * std::exp (-3.0 * (b - pre) / T);
                wetStroke.lineTo (xb (b), BASE - H * (float) env);
            }
            wetFill = wetStroke;
            wetFill.lineTo (xb (8.0), BASE);
            wetFill.closeSubPath();
        }
        else if (fx == 7)
        {
            const float w = xb (beat) - X0;
            for (int k = 0; k * beat < 4.0 - 1e-9; ++k)
                bar (wetFill, xb (k * beat) + 1.0f, jmax (2.0f, w * (0.15f + 0.85f * param) - 2.0f),
                     BASE - H * depth * 0.9f, BASE);
            wetStroke.startNewSubPath (xb (0), BASE - H * depth * 0.9f);
            wetStroke.lineTo (xb (4), BASE - H * depth * 0.9f);
            dry.clear();
        }
    }
    else
    {
        for (int dec = 20; dec < 20000; dec *= 10)
            for (int i = 1; i < 10; ++i)
                if (dec * i <= 20000)
                {
                    gridMinor.startNewSubPath (xf (dec * i), TOP - 10.0f);
                    gridMinor.lineTo (xf (dec * i), BASE);
                }
        for (int db : { 12, -12, -24 })
        {
            gridMinor.startNewSubPath (X0, ydb (db));
            gridMinor.lineTo (X1, ydb (db));
        }
        gridMajor.startNewSubPath (X0, ydb (0));
        gridMajor.lineTo (X1, ydb (0));
        for (int f : { 50, 100, 200, 500, 1000, 2000, 5000, 10000 })
            labels.push_back ({ freqLabel (f), xf (f) });

        constexpr int M = 600;
        for (int i = 0; i < M; ++i)
        {
            const double f = 20.0 * std::pow (10.0, 3.0 * i / (M - 1));
            double db = 0.0;
            if (fx == 6)
            {
                const double fc = 20000.0 * std::pow (100.0 / 20000.0, depth), Q = 0.7 + param * 7.0, r = f / fc;
                db = -10.0 * std::log10 (std::pow (1.0 - r * r, 2.0) + std::pow (r / Q, 2.0));
            }
            else if (fx == 4)
            {
                const double tau = (0.6 + depth * 7.0) / 1000.0, gg = 0.35 + 0.6 * param;
                db = 20.0 * std::log10 (std::sqrt (1.0 + gg * gg + 2.0 * gg * std::cos (MathConstants<double>::twoPi * f * tau)) + 1e-4);
            }
            else
            {
                const double L = std::log10 (f), f0 = 200.0 * std::pow (20.0, depth);
                for (double m : { 1.0, 2.6, 6.1, 13.0 })
                {
                    const double Ln = std::log10 (f0 * m);
                    db += -26.0 * std::exp (-std::pow (L - Ln, 2.0) / 0.009)
                          + param * 9.0 * std::exp (-std::pow (L - Ln - 0.2, 2.0) / 0.008);
                }
            }
            if (i == 0) wetStroke.startNewSubPath (xf (f), ydb (db));
            else        wetStroke.lineTo (xf (f), ydb (db));
        }
        wetFill = wetStroke;
        wetFill.lineTo (X1, BASE);
        wetFill.lineTo (X0, BASE);
        wetFill.closeSubPath();
    }

    g.setColour (Theme::gridMinor);
    g.strokePath (gridMinor, PathStrokeType (1.0f));
    g.setColour (Theme::gridMajor);
    g.strokePath (gridMajor, PathStrokeType (1.0f));
    const bool taps = fx == 0 || fx == 1 || fx == 2 || fx == 7;   // discrete repeats read as solid bars
    g.setColour (Theme::accentLight.withAlpha (taps ? (on ? 0.85f : 0.4f) : (on ? 0.26f : 0.10f)));
    g.fillPath (wetFill);
    g.setColour (Theme::accentLight.withAlpha (on ? 1.0f : 0.35f));
    g.strokePath (wetStroke, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Theme::textValue.withAlpha (0.85f));
    g.fillPath (dry);

    g.setFont (Theme::font (11.0f));
    g.setColour (Theme::muted);
    for (auto& l : labels)
        g.drawText (l.text, Rectangle<float> (l.x - 24.0f, 272.0f, 48.0f, 14.0f), Justification::centred);
    g.setFont (Theme::font (10.0f));
    g.drawText ("IN", Rectangle<float> (906.0f, 272.0f, 22.0f, 14.0f), Justification::centred);
    g.drawText ("OUT", Rectangle<float> (930.0f, 272.0f, 26.0f, 14.0f), Justification::centred);

    // title + info line
    g.setColour (Theme::accentLight);
    g.setFont (Theme::font (38.0f, "Regular", 0.02f));
    g.drawText (Params::fxNames[jlimit (0, 7, fx)], Rectangle<float> (16.0f, 8.0f, 600.0f, 44.0f), Justification::centredLeft);

    const String pct = String (roundToInt (param * 100.0f)) + "%";
    const String pname = Params::paramNameFor (fx).toLowerCase();
    const String beatName = Params::beatNames[jlimit (0, 9, beatIdx)];
    const String sep = String::fromUTF8 ("  \xc2\xb7  ");
    String info;
    if (isTime) info = beatName + " beat" + sep + String (roundToInt (beat * msPerBeat)) + " ms" + sep + pname + " " + pct;
    else        info = beatName + " beat cycle" + sep + String (beat * msPerBeat / 1000.0, 2) + " s" + sep + pname + " " + pct;
    g.setColour (Theme::subtle);
    g.setFont (Theme::font (13.0f));
    g.drawText (info, Rectangle<float> (16.0f, 54.0f, 700.0f, 16.0f), Justification::centredLeft);

    // chips, right-aligned at x = 910
    auto chip = [&] (const String& t, float right, bool filled)
    {
        const auto f = Theme::font (11.0f, filled ? "Medium" : "Regular", 0.06f);
        const float w = Theme::textWidth (f, t) + 16.0f;
        Rectangle<float> r (right - w, 14.0f, w, 22.0f);
        g.setColour (filled ? Theme::accent : Theme::meterTrack);
        g.fillRoundedRectangle (r, 4.0f);
        if (! filled) { g.setColour (Theme::border); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f); }
        g.setColour (filled ? Colours::white : Theme::subtle);
        g.setFont (f);
        g.drawText (t, r, Justification::centred);
        return r.getX();
    };
    const float x = chip (on ? "WET" : "BYPASS", 910.0f, on);
    chip ("HOST " + String (bpm, 2) + " BPM", x - 8.0f, false);
}
