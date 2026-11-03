#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// CORE8 effect engine: eight DJM-style beat effects, synced to the host tempo.
enum FxType { Echo = 0, Delay, PingPong, Reverb, Flanger, Phaser, Filter, Roll, NumFx };

struct HostInfo
{
    double bpm = 124.0;
    double ppq = 0.0;
    bool ppqValid = false;
    bool playing = false;
};

struct FxParams
{
    int fx = Echo;
    double beats = 0.5;      // beat length in quarter notes
    float depth = 0.5f;      // Level / Depth
    float param = 0.5f;      // effect-specific (feedback, decay, resonance, gate)
    float tone = 0.5f;       // 0 = LPF, 0.5 = flat, 1 = HPF on the wet signal
    float outputDb = 0.0f;
    bool spill = true;       // tails ring out after FX off (send-type effects)
};

class FxEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // targetOn = the FX ON switch. With quantize, the switch lands on the next beat grid line.
    void process (juce::AudioBuffer<float>& buffer, const FxParams& p, const HostInfo& host,
                  bool targetOn, bool quantize);

    bool isActive() const noexcept { return active; }

private:
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        void reset() noexcept { ic1 = ic2 = 0.0f; }
        inline void process (float x, float g, float k, float& low, float& band, float& high) noexcept
        {
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            low = v2; band = v1; high = x - k * v1 - v2;
        }
    };

    void clearEffectState();
    // Reads behind the write head; anything older than `filled` samples counts as silence,
    // which lets clearEffectState() run in O(1) instead of zeroing megabytes on the audio thread.
    static float readInterp (const std::vector<float>& buf, int writePos, float delaySamples, int filled) noexcept;
    float computeLfo (double phase) const noexcept;

    double sr = 44100.0;
    bool active = false;
    int lastFx = -1;

    float sendEnv = 0.0f, wetEnv = 0.0f, mixEnv = 0.0f, envCoeff = 0.01f;
    juce::SmoothedValue<float> depthS, gainS, toneS;
    bool smoothersPrimed = false;   // first block jumps straight to the parameter values

    // idle bypass: nothing to compute when the effect is off and its tail has died
    // A send effect is only idle once its output has been silent for longer than one full delay
    // period (or pre-delay plus decay margin for reverb): only then can the line hold nothing.
    juce::int64 silentSamples = 0;
    bool tailSilent = true;

    // echo / delay / ping pong
    std::vector<float> dl[2];
    int dlW = 0, dlFilled = 0;
    float dSmooth = -1.0f;
    float fbLp[2] {};

    // reverb
    std::vector<float> preBuf[2];
    int preW = 0, preFilled = 0;
    juce::Reverb reverb;
    std::vector<float> scrL, scrR, scrEnv, scrGain;

    // flanger
    std::vector<float> md[2];
    int mdW = 0, mdFilled = 0;

    // phaser
    float ap[2][6] {};
    float phLast[2] {};

    // filter + wet tone; coefficients are updated at control rate and ramped per sample
    Svf filt[2], tone[2];
    float toneGCur = 1.0f, filtGCur = 1.0f, phACur = 0.0f, flDsCur = 0.0f;
    bool coeffsPrimed = false;

    // roll
    std::vector<float> rb[2];
    int rbW = 0, rollStart = 0;
    juce::int64 rollT = 0;

    double freePhase = 0.0;
};
