#include "FxEngine.h"
#include <cmath>

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr int kControl = 16;            // control-rate step for LFOs and filter coefficients
    constexpr float kSilence = 1.0e-5f;     // about -100 dBFS

    inline void allocate (std::vector<float>& v, size_t n) { v.assign (n, 0.0f); }
    inline void zero (std::vector<float>& v) { std::fill (v.begin(), v.end(), 0.0f); }
    inline bool isSendType (int fx) { return fx == Echo || fx == Delay || fx == PingPong || fx == Reverb; }
    inline bool usesLfo (int fx) { return fx == Flanger || fx == Phaser || fx == Filter; }
}

void FxEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    envCoeff = 1.0f - std::exp (-1.0f / (0.008f * (float) sr));

    for (int ch = 0; ch < 2; ++ch)
    {
        allocate (dl[ch], (size_t) (sr * 10.0) + 8);     // up to 10 s of echo time
        allocate (preBuf[ch], (size_t) (sr * 0.6) + 8);  // reverb pre-delay
        allocate (md[ch], (size_t) (sr * 0.02) + 8);     // flanger
        allocate (rb[ch], (size_t) (sr * 10.0) + 8);     // roll capture
    }

    const auto n = (size_t) juce::jmax (1, maxBlockSize);
    allocate (scrL, n); allocate (scrR, n); allocate (scrEnv, n); allocate (scrGain, n);

    reverb.setSampleRate (sr);
    depthS.reset (sr, 0.02);
    gainS.reset (sr, 0.02);
    toneS.reset (sr, 0.03);
    reset();
}

void FxEngine::reset()
{
    clearEffectState();
    rbW = rollStart = 0;
    rollT = 0;
    active = false;
    sendEnv = wetEnv = mixEnv = 0.0f;
    freePhase = 0.0;
    silentSamples = 0;
    tailSilent = true;
    smoothersPrimed = false;
}

void FxEngine::clearEffectState()
{
    // O(1): the fill counters make old buffer contents read as silence.
    dlFilled = preFilled = mdFilled = 0;
    dlW = preW = mdW = 0;
    for (int ch = 0; ch < 2; ++ch)
    {
        fbLp[ch] = phLast[ch] = 0.0f;
        for (auto& s : ap[ch]) s = 0.0f;
        filt[ch].reset();
        tone[ch].reset();
    }
    dSmooth = -1.0f;
    coeffsPrimed = false;
    reverb.reset();
}

float FxEngine::readInterp (const std::vector<float>& buf, int writePos, float delaySamples, int filled) noexcept
{
    if (delaySamples >= (float) (filled - 1))
        return 0.0f;

    const int size = (int) buf.size();
    float rp = (float) writePos - delaySamples;
    if (rp < 0.0f) rp += (float) size;
    int i0 = (int) rp;
    const float fr = rp - (float) i0;
    if (i0 >= size) i0 -= size;
    int i1 = i0 + 1;
    if (i1 >= size) i1 -= size;
    return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
}

float FxEngine::computeLfo (double phase) const noexcept
{
    return 0.5f - 0.5f * std::cos (twoPi * (float) phase);
}

void FxEngine::process (juce::AudioBuffer<float>& buffer, const FxParams& p, const HostInfo& host,
                        bool targetOn, bool quantize)
{
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2)
        return;

    if ((size_t) n > scrL.size())
    {
        allocate (scrL, (size_t) n); allocate (scrR, (size_t) n);
        allocate (scrEnv, (size_t) n); allocate (scrGain, (size_t) n);
    }

    if (p.fx != lastFx)
    {
        clearEffectState();
        lastFx = p.fx;
    }

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    const double bpm = juce::jlimit (20.0, 999.0, host.bpm);
    const double spb = 60.0 / bpm * sr;                 // samples per beat
    const double beats = juce::jmax (1.0 / 64.0, p.beats);
    const double ppqPerSample = bpm / 60.0 / sr;
    const bool synced = host.playing && host.ppqValid;
    const bool sendType = isSendType (p.fx);

    // ---- quantized FX ON / OFF -------------------------------------------------------
    int switchAt = -1;
    if (targetOn != active)
    {
        if (! quantize || ! synced)
            switchAt = 0;
        else
        {
            const double grid = juce::jmin (beats, 1.0);
            const double next = std::ceil (host.ppq / grid - 1.0e-9) * grid;
            const int offset = (int) std::floor ((next - host.ppq) / ppqPerSample);
            if (offset < n)
                switchAt = juce::jmax (0, offset);
        }
    }

    const float gainTarget = juce::Decibels::decibelsToGain (p.outputDb);
    if (! smoothersPrimed)
    {
        depthS.setCurrentAndTargetValue (p.depth);
        gainS.setCurrentAndTargetValue (gainTarget);
        toneS.setCurrentAndTargetValue (p.tone);
        smoothersPrimed = true;
    }
    depthS.setTargetValue (p.depth);
    gainS.setTargetValue (gainTarget);
    toneS.setTargetValue (p.tone);

    // ---- idle bypass -------------------------------------------------------------------
    // Off, envelopes closed and the tail has decayed below -100 dB: just apply output gain.
    const bool tailDone = ! sendType || ! p.spill || tailSilent;
    if (! active && switchAt < 0 && mixEnv < kSilence && sendEnv < kSilence && tailDone)
    {
        if (mixEnv > 0.0f || sendEnv > 0.0f)
            clearEffectState();   // start the next transition from clean state
        mixEnv = sendEnv = 0.0f;
        wetEnv = p.spill ? 1.0f : 0.0f;
        depthS.skip (n);
        toneS.skip (n);

        if (gainS.isSmoothing())
        {
            for (int i = 0; i < n; ++i)
            {
                const float g = gainS.getNextValue();
                L[i] *= g; R[i] *= g;
            }
        }
        else if (std::abs (gainS.getTargetValue() - 1.0f) > 1.0e-6f)
        {
            buffer.applyGain (0, 0, n, gainS.getTargetValue());
            buffer.applyGain (1, 0, n, gainS.getTargetValue());
        }
        return;
    }

    // ---- per-effect block constants ----------------------------------------------------
    const int dlSize = (int) dl[0].size();
    const float dTarget = (float) juce::jlimit (1.0, (double) dlSize - 4.0, beats * spb);
    if (dSmooth < 0.0f) dSmooth = dTarget;
    const float dCoeff = 1.0f - std::exp (-1.0f / (0.03f * (float) sr));

    float fb = 0.0f;
    if (p.fx == Echo)     fb = 0.30f + 0.62f * p.param;
    if (p.fx == Delay)    fb = 0.05f + 0.35f * p.param;
    if (p.fx == PingPong) fb = 0.25f + 0.60f * p.param;
    const float echoLpC = 1.0f - std::exp (-twoPi * 5000.0f / (float) sr);

    const int preSize = (int) preBuf[0].size();
    const int preD = (int) juce::jlimit (1.0, (double) preSize - 2.0, juce::jmin (beats * spb, 0.5 * sr));
    if (p.fx == Reverb)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.5f + 0.48f * p.param;
        rp.damping = 0.35f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
    }

    const int mdSize = (int) md[0].size();
    const float flFb = 0.30f + 0.60f * p.param;
    const float phFb = 0.70f * p.param;
    const float filtK = 1.0f / (0.7f + p.param * 7.0f);
    const float toneK = 1.41421356f;

    const int rbSize = (int) rb[0].size();
    const int slice = (int) juce::jlimit (64.0, (double) rbSize - 2.0, beats * spb);
    const float fade = (float) (0.002 * sr);
    const float gateLen = juce::jmax (fade * 2.0f, (float) slice * (0.15f + 0.85f * p.param));

    const bool lfoFx = usesLfo (p.fx);
    float wetPeak = 0.0f;

    // ---- control-rate chunks -----------------------------------------------------------
    for (int c0 = 0; c0 < n; c0 += kControl)
    {
        const int c1 = juce::jmin (n, c0 + kControl);
        const float invLen = 1.0f / (float) (c1 - c0);

        // wet tone: smoothed value, coefficient ramped across the chunk
        const float toneVal = toneS.skip (c1 - c0);
        const float tv = (toneVal - 0.5f) * 2.0f;
        const int toneMode = std::abs (tv) < 0.02f ? 0 : (tv < 0.0f ? 1 : 2);
        float toneG = toneGCur;
        if (toneMode != 0)
        {
            const float fc = tv < 0.0f ? 20000.0f * std::pow (300.0f / 20000.0f, -tv)
                                       : 20.0f * std::pow (150.0f, tv);
            toneG = std::tan (pi * juce::jmin (fc, 0.45f * (float) sr) / (float) sr);
        }

        // LFO-driven coefficients at the chunk end
        float flDsT = flDsCur, phAT = phACur, filtGT = filtGCur;
        if (lfoFx)
        {
            double phase;
            if (synced)
            {
                const double pos = (host.ppq + (double) c1 * ppqPerSample) / beats;
                phase = pos - std::floor (pos);
            }
            else
            {
                freePhase += (double) (c1 - c0) / (beats * spb);
                freePhase -= std::floor (freePhase);
                phase = freePhase;
            }
            const float lfo = computeLfo (phase);
            const float d = depthS.getTargetValue();

            if (p.fx == Flanger)
                flDsT = (0.6f + (0.6f + 6.4f * d) * lfo) * 0.001f * (float) sr;
            else if (p.fx == Phaser)
            {
                const float fc = juce::jlimit (40.0f, 0.45f * (float) sr,
                                               1000.0f * std::pow (2.0f, (lfo * 2.0f - 1.0f) * (1.0f + 3.0f * d)));
                const float t = std::tan (pi * fc / (float) sr);
                phAT = (t - 1.0f) / (t + 1.0f);
            }
            else
            {
                const float fc = juce::jlimit (40.0f, 0.45f * (float) sr,
                                               20000.0f * std::pow (100.0f / 20000.0f, d * lfo));
                filtGT = std::tan (pi * fc / (float) sr);
            }
        }

        if (! coeffsPrimed)
        {
            toneGCur = toneG; flDsCur = flDsT; phACur = phAT; filtGCur = filtGT;
            coeffsPrimed = true;
        }
        const float toneStep = (toneG - toneGCur) * invLen;
        const float flStep = (flDsT - flDsCur) * invLen;
        const float phStep = (phAT - phACur) * invLen;
        const float fgStep = (filtGT - filtGCur) * invLen;

        auto toneProc = [&] (int ch, float x) noexcept
        {
            if (toneMode == 0) return x;
            float lo, ba, hi;
            tone[ch].process (x, toneGCur, toneK, lo, ba, hi);
            return toneMode == 1 ? lo : hi;
        };

        for (int i = c0; i < c1; ++i)
        {
            if (i == switchAt)
            {
                active = targetOn;
                if (active && p.fx == Roll)
                {
                    rollStart = rbW;
                    rollT = 0;
                }
            }

            toneGCur += toneStep;
            flDsCur += flStep;
            phACur += phStep;
            filtGCur += fgStep;

            const float d = depthS.getNextValue();
            const float gain = gainS.getNextValue();
            const float on = active ? 1.0f : 0.0f;
            sendEnv += (on - sendEnv) * envCoeff;
            wetEnv  += ((active || p.spill ? 1.0f : 0.0f) - wetEnv) * envCoeff;
            mixEnv  += (on - mixEnv) * envCoeff;

            const float xL = L[i], xR = R[i];
            float oL = xL, oR = xR;

            switch (p.fx)
            {
                case Echo:
                case Delay:
                {
                    dSmooth += (dTarget - dSmooth) * dCoeff;
                    const float x[2] { xL, xR };
                    float o[2];
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const float w = readInterp (dl[ch], dlW, dSmooth, dlFilled);
                        float fbSig = w;
                        if (p.fx == Echo)
                        {
                            fbLp[ch] += (w - fbLp[ch]) * echoLpC;   // echoes darken as they repeat
                            fbSig = fbLp[ch];
                        }
                        dl[ch][(size_t) dlW] = x[ch] * sendEnv + fb * fbSig;
                        const float wet = toneProc (ch, w) * d * wetEnv;
                        wetPeak = juce::jmax (wetPeak, std::abs (wet));
                        o[ch] = x[ch] + wet;
                    }
                    oL = o[0]; oR = o[1];
                    if (++dlW >= dlSize) dlW = 0;
                    if (dlFilled < dlSize) ++dlFilled;
                    break;
                }

                case PingPong:
                {
                    dSmooth += (dTarget - dSmooth) * dCoeff;
                    const float wL = readInterp (dl[0], dlW, dSmooth, dlFilled);
                    const float wR = readInterp (dl[1], dlW, dSmooth, dlFilled);
                    const float in = 0.5f * (xL + xR) * sendEnv;
                    dl[0][(size_t) dlW] = in + fb * wR;
                    dl[1][(size_t) dlW] = fb * wL;
                    const float eL = toneProc (0, wL) * d * wetEnv;
                    const float eR = toneProc (1, wR) * d * wetEnv;
                    wetPeak = juce::jmax (wetPeak, std::abs (eL), std::abs (eR));
                    oL = xL + eL;
                    oR = xR + eR;
                    if (++dlW >= dlSize) dlW = 0;
                    if (dlFilled < dlSize) ++dlFilled;
                    break;
                }

                case Reverb:
                {
                    int rp = preW - preD;
                    if (rp < 0) rp += preSize;
                    const bool valid = preD < preFilled;
                    scrL[(size_t) i] = valid ? preBuf[0][(size_t) rp] : 0.0f;
                    scrR[(size_t) i] = valid ? preBuf[1][(size_t) rp] : 0.0f;
                    preBuf[0][(size_t) preW] = xL * sendEnv;
                    preBuf[1][(size_t) preW] = xR * sendEnv;
                    if (++preW >= preSize) preW = 0;
                    if (preFilled < preSize) ++preFilled;
                    scrEnv[(size_t) i] = d * wetEnv;
                    scrGain[(size_t) i] = gain;
                    break;
                }

                case Flanger:
                {
                    const float x[2] { xL, xR };
                    float o[2];
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const float w = readInterp (md[ch], mdW, flDsCur, mdFilled);
                        md[ch][(size_t) mdW] = x[ch] + flFb * w;
                        const float y = 0.5f * (x[ch] + w);
                        o[ch] = x[ch] + (toneProc (ch, y) - x[ch]) * mixEnv;
                    }
                    oL = o[0]; oR = o[1];
                    if (++mdW >= mdSize) mdW = 0;
                    if (mdFilled < mdSize) ++mdFilled;
                    break;
                }

                case Phaser:
                {
                    const float a = phACur;
                    const float x[2] { xL, xR };
                    float o[2];
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        float y = x[ch] + phFb * phLast[ch];
                        for (int s = 0; s < 6; ++s)
                        {
                            const float out = a * y + ap[ch][s];
                            ap[ch][s] = y - a * out;
                            y = out;
                        }
                        phLast[ch] = y;
                        const float wet = 0.5f * (x[ch] + y);
                        o[ch] = x[ch] + (toneProc (ch, wet) - x[ch]) * mixEnv;
                    }
                    oL = o[0]; oR = o[1];
                    break;
                }

                case Filter:
                {
                    const float x[2] { xL, xR };
                    float o[2];
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        float lo, ba, hi;
                        filt[ch].process (x[ch], filtGCur, filtK, lo, ba, hi);
                        o[ch] = x[ch] + (toneProc (ch, lo) - x[ch]) * mixEnv;
                    }
                    oL = o[0]; oR = o[1];
                    break;
                }

                case Roll:
                {
                    // Capture only while a roll is recording its first slice; nothing else reads it.
                    if (active && rollT < slice)
                    {
                        rb[0][(size_t) rbW] = xL;
                        rb[1][(size_t) rbW] = xR;
                        if (++rbW >= rbSize) rbW = 0;
                    }
                    if (active || mixEnv > 1.0e-4f)
                    {
                        const int pos = (int) (rollT % slice);
                        int idx = rollStart + pos;
                        if (idx >= rbSize) idx -= rbSize;
                        const float env = juce::jlimit (0.0f, 1.0f,
                                                        juce::jmin ((float) pos / fade, (gateLen - (float) pos) / fade));
                        const float wL = rb[0][(size_t) idx] * env;
                        const float wR = rb[1][(size_t) idx] * env;
                        ++rollT;
                        const float m = mixEnv * d;
                        oL = xL * (1.0f - m) + toneProc (0, wL) * m;
                        oR = xR * (1.0f - m) + toneProc (1, wR) * m;
                    }
                    break;
                }

                default: break;
            }

            L[i] = oL * gain;
            R[i] = oR * gain;
        }
    }

    // Reverb runs as a block after the pre-delay pass. The tone filter here uses the
    // end-of-block coefficient, which is fine for a diffuse tail.
    if (p.fx == Reverb)
    {
        reverb.processStereo (scrL.data(), scrR.data(), n);
        const float tv = (toneS.getCurrentValue() - 0.5f) * 2.0f;
        const int toneMode = std::abs (tv) < 0.02f ? 0 : (tv < 0.0f ? 1 : 2);
        for (int i = 0; i < n; ++i)
        {
            const auto si = (size_t) i;
            float wl = scrL[si], wr = scrR[si];
            if (toneMode != 0)
            {
                float lo, ba, hi;
                tone[0].process (wl, toneGCur, toneK, lo, ba, hi); wl = toneMode == 1 ? lo : hi;
                tone[1].process (wr, toneGCur, toneK, lo, ba, hi); wr = toneMode == 1 ? lo : hi;
            }
            wl *= scrEnv[si] * scrGain[si];
            wr *= scrEnv[si] * scrGain[si];
            wetPeak = juce::jmax (wetPeak, std::abs (wl), std::abs (wr));
            // dry was already scaled by the output gain in the sample loop
            L[i] += wl;
            R[i] += wr;
        }
    }

    // tail tracking for the idle bypass
    const double needSilent = (p.fx == Reverb ? (double) preD + 0.1 * sr : (double) dTarget) + (double) n;
    if (active || sendEnv > kSilence || wetPeak > kSilence)
    {
        silentSamples = 0;
        tailSilent = false;
    }
    else
    {
        silentSamples += n;
        tailSilent = (double) silentSamples > needSilent;
    }
}
