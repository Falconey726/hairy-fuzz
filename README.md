# Hairy Fuzz

A 70s-style fuzz Audio Unit (and VST3) for Ableton Live, with a stomp-box interface.

![Hairy Fuzz](docs/editor.png)

## Controls

| Knob | What it does |
|------|--------------|
| **Fuzz** | Drive into two cascaded clipping stages. The first is a biased, lopsided transistor stage (gritty, a little sputtery); the second is a diode-style clipper (thick and saturated). |
| **Filter** | Works like a RAT filter: turning it up makes the tone darker (14 kHz down to about 450 Hz). |
| **Volume** | Output level. |
| **Footswitch** | Bypass with a short crossfade. The red LED is lit when the effect is on. |

The fuzz runs at 4x oversampling to keep the top end smooth instead of aliasing.

## Getting the plugin

Every push builds the plugin on a GitHub Mac runner and checks it with Apple's `auval`.
Open the **Actions** tab, click the newest green run, and download **HairyFuzz-macOS**
from the Artifacts section. Then follow `INSTALL.txt` inside the zip.

## Building yourself (macOS)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
```

The plugin bundles land in `build/HairyFuzz_artefacts/Release/`.

Built with [JUCE](https://juce.com) (downloaded automatically by CMake).
