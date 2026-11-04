# IIC+ NAM Standalone — audio-engine starter

Windows-first C++17 / JUCE starter for a standalone IIC+-inspired amp simulator.

## What is in this first milestone

- JUCE standalone audio device callback and an in-app audio-device settings dialog.
- Two-channel input initialization; defaults to input channel 2 for the common Scarlett Solo guitar-input setup, falling back to channel 1 if only one input is available.
- Mono processed signal sent to the selected output channels (same signal on left/right).
- Load a `.nam` profile using the official NeuralAmpModelerCore loader.
- Load a `.wav` cabinet impulse response using the same core's linear-model WAV loader.
- Input/output gain controls.
- Five-band post-model graphic EQ at 80, 240, 750, 2200 and 6600 Hz.
- Early approximation switches are represented in the engine API; the faceplate and switch controls will be expanded in the next milestone.
- Dark, gold-trimmed faceplate placeholder UI.

**Important modeling note:** a standard NAM capture is not intrinsically a parametric IIC+ model. The five-band EQ and bright/deep approximations in this starter are external DSP controls. They do not recreate the exact original circuit or change the captured amp's internal gain/tone-stack behavior. Also, if your NAM profile already includes a cabinet or the amp graphic EQ, adjust the chain to avoid doubling those stages.

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 with the **Desktop development with C++** workload
- CMake 3.22+
- Git
- An audio interface and ASIO driver recommended for low-latency playing (WASAPI is available through JUCE as well)

## Build

Open PowerShell in this folder:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\setup.ps1
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The executable is produced under a path similar to:

`build\IICPlusNAM_artefacts\Release\IIC+ NAM.exe`

JUCE and NeuralAmpModelerCore are cloned from their official public repositories during setup. Keep their licenses and notices with any redistributed build. NeuralAmpModelerCore is MIT-licensed; review JUCE's current licensing terms for your intended distribution.

## First run

1. Connect your guitar to the instrument input of your audio interface.
2. Select the interface as the active Windows audio device. Use the interface's ASIO driver where available.
3. Launch the application.
4. Click **AUDIO SETTINGS** and select the Focusrite/Scarlett input and output device. For the Scarlett Solo guitar jack on input 2, the engine defaults to the second input channel. If using ASIO, choose the Focusrite USB ASIO driver if listed.
5. Set the output device to the Scarlett so the processed sound reaches your monitor connected to its left output.
6. Click **LOAD NAM PROFILE** and select your `.nam` file.
7. Click **LOAD CAB IR** and select a **mono** `.wav` IR. Stereo IR routing is not implemented in this starter yet; use a mono IR for now.
8. Set the interface input gain conservatively, then use the application's input/output controls.
9. Start with output at -12 dB and raise it carefully.

## Current limitations / next engineering pass

This is a starter implementation, not yet a verified release build. It has not been compiled in this environment. Before relying on it for live use, we should compile it on your Windows machine and fix any version-specific dependency/API differences.

Next items:
- Verify NAM target/header names against the exact upstream revision.
- Report audio sample rate/buffer size and add a visible input-channel selector (currently defaults to channel 2).
- Move model swaps through a real-time-safe handoff so loading cannot contend with the audio callback.
- Add smoothing for controls, stereo IR support, meters, clipping protection and bypass.
- Add the full authentic front panel: gain/tone/lead controls, pull switches, five-band EQ sliders and MIDI Program Change preset recall.
- Add JSON preset save/load.
