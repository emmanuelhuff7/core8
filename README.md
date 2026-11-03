# CORE8 by zvle

DJM-style transition effects as a VST3 / AU plugin for macOS: Echo, Delay, Ping Pong, Reverb,
Flanger, Phaser, Filter and Roll, all locked to your DAW's tempo.

## Build on your Mac (about 10 minutes the first time)

1. Install Xcode from the App Store, open it once to accept the license, then in Terminal:
   ```
   xcode-select --install
   ```
2. Install CMake:
   ```
   brew install cmake
   ```
   (No Homebrew? Download CMake from cmake.org and add it to your PATH.)
3. Unzip this folder, then from inside it:
   ```
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release -j 8
   ```
   The first run downloads JUCE 8.0.4 automatically.

The build installs the plugins for you:
- `~/Library/Audio/Plug-Ins/VST3/CORE8.vst3`
- `~/Library/Audio/Plug-Ins/Components/CORE8.component` (AU)

It builds a universal binary, so it runs natively on Apple Silicon and Intel.

Optional AU check: `auval -v aufx Cor8 Zvle`

## Load it in FL Studio

1. Options > Manage plugins > **Find installed plugins**.
2. Put CORE8 on the mixer insert of the track you are transitioning out of.
3. Pick a preset from the CORE8 preset menu (or FL's preset arrows).

## Making transitions in the DAW

**Ride is the one lane to automate.** Ride and FX On sit at the top of FL's parameter list.
- Ride at 0: effect off. Anything above 0 switches it on and scales Level/Depth by the Ride value,
  so one drawn ramp (0 to 100% over the last 8 bars, then back to 0) is a complete transition.
- To make the lane: drag the Ride bar, then Tools > Last tweaked > Create automation clip.
- When Ride returns to 0, spillover tails keep the last Ride level, so a sharp cut leaves a clean echo-out.
- **FX On** is the hard switch (overrides Ride, full depth). Use it for on/off hits.
- With **Quantize** on, both snap to the effect's beat grid even if your automation is slightly early.
- **Spillover** on: echo, delay and reverb tails keep ringing after FX On goes off, and after the
  audio clip ends. That is how Echo Out and Reverb Wash work: end the clip at the phrase and let the
  tail carry into the next track.
- **Hold** mode makes the FX ON button momentary (press and hold), like Roll on a DJM.
- For a shortening roll build, automate **Beat** 1/4 > 1/8 > 1/16 over the last bar of Drop Roll.
- Map any knob to a controller with FL's Tools > Last tweaked > Link to controller.

## CPU

CORE8 is built to cost nothing when it isn't doing anything:
- **Idle bypass:** when the effect is off and its tail has fully decayed (below -100 dB for longer
  than one delay period), processing stops and audio passes through bit-exact.
- Filters and LFO sweeps update at control rate (every 16 samples) with per-sample ramps.
- Switching effects no longer clears megabytes of buffers on the audio thread (O(1) reset).
- The UI caches its display and only redraws what changed; meters redraw on their own.

Measured on one core at 48 kHz: about 0.1-0.3% while an effect runs, near 0% when idle.
In FL Studio you can also turn on **Smart disable** for the insert (mixer slot menu) so FL skips the
plugin entirely while the track is silent. CORE8 reports a 12 s tail to the host, but check one
echo-out with it on; if FL cuts the tail, leave Smart disable off for that insert.

## Factory presets

| Preset | Effect | Use |
|---|---|---|
| Echo Out | Echo 1/2, feedback 72%, HPF 30% | Exit the outgoing track on its last phrase |
| Reverb Wash | Reverb, pre-delay 1/4, decay 90%, HPF 40% | Wash out on the last snare, drop into the tail |
| Jet Sweep | Flanger, 16-beat cycle, feedback 75% | Sweep through a breakdown |
| Fade to Dark | Filter, 16-beat cycle, resonance 45% | Low-pass the old track as the new one comes in |
| Drop Roll | Roll 1/4, gate 90%, Hold mode | Hold on the last bar, release on the 1 |

## Controls

- Drag knobs up/down; hold Shift for fine control; double-click resets to the preset value.
- A / B compares two settings; undo / redo covers every change.
- Window is resizable (keeps its proportions).

## Project layout

- `Source/DSP/FxEngine.*` the eight effects, tempo sync, quantize, spillover
- `Source/PluginProcessor.*` parameters, presets, A/B, state
- `Source/PluginEditor.*`, `Source/UI/*` the interface (Helvetica Neue, #4d4775 highlights)
