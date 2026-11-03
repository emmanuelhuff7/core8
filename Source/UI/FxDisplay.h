#pragma once

#include "Theme.h"

class Core8AudioProcessor;

// The analyzer-style display. The static layer (grid, curves, labels, title) is cached as an image
// and only re-rendered when a parameter it shows changes; the level meters are a small overlay
// that repaints on its own, so the 30 Hz UI refresh costs almost nothing.
class FxDisplay : public juce::Component
{
public:
    explicit FxDisplay (Core8AudioProcessor& p);

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Called from the editor timer: repaints only what actually changed.
    void refresh();

private:
    struct State
    {
        int fx = -1, beat = -1, depthQ = -1, paramQ = -1, bpmQ = -1;
        bool on = false;
        bool operator== (const State& o) const
        {
            return fx == o.fx && beat == o.beat && depthQ == o.depthQ && paramQ == o.paramQ && bpmQ == o.bpmQ && on == o.on;
        }
        bool operator!= (const State& o) const { return ! (*this == o); }
    };

    class Meters : public juce::Component
    {
    public:
        explicit Meters (Core8AudioProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); setOpaque (true); }
        void paint (juce::Graphics& g) override;
        void refresh();

    private:
        Core8AudioProcessor& proc;
        int inH = -1, outH = -1;
        bool on = false;
    };

    State readState() const;

    Core8AudioProcessor& proc;
    State shown;
    Meters meters;
};
