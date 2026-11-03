#pragma once

// Factory presets. fx: 0 Echo, 1 Delay, 2 Ping Pong, 3 Reverb, 4 Flanger, 5 Phaser, 6 Filter, 7 Roll
// beat index: 0 1/16, 1 1/8, 2 1/4, 3 1/2, 4 3/4, 5 1, 6 2, 7 4, 8 8, 9 16
struct Preset
{
    const char* name;
    const char* use;
    int fx, beat;
    float depth, param, tone, outDb;
    bool hold, spill, quant;
};

inline constexpr Preset factoryPresets[] {
    { "Init",         "Neutral starting point",                          0, 3, 0.50f, 0.50f, 0.50f,  0.0f, false, true,  true },
    { "Echo Out",     "Exit the outgoing track on its last phrase",      0, 3, 0.62f, 0.72f, 0.65f,  0.0f, false, true,  true },
    { "Reverb Wash",  "Wash out on the last snare, drop into the tail",  3, 2, 0.80f, 0.90f, 0.70f, -1.0f, false, true,  true },
    { "Jet Sweep",    "16-beat flanger through a breakdown",             4, 9, 0.55f, 0.75f, 0.50f,  0.0f, false, true,  true },
    { "Fade to Dark", "Low-pass the old track as the new one enters",    6, 9, 0.78f, 0.45f, 0.50f,  0.0f, false, false, true },
    { "Drop Roll",    "Hold on the last bar, release on the 1",          7, 2, 1.00f, 0.90f, 0.50f,  0.0f, true,  false, true },
};

inline constexpr int numFactoryPresets = (int) (sizeof (factoryPresets) / sizeof (factoryPresets[0]));
