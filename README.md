# MoteField by Rango Labs

MoteField turns incoming audio into layered micro loops, granular textures, rhythmic glitches, and spatial repeats. It combines **11 effect modes**, A-D variations, **32 factory presets**, and a **60-second phrase looper** in a compact hardware-style interface.

The worn cream enclosure, organic black print, and reactive Rango Labs mark surround a live ferrofluid-inspired display. The material follows the processed sound and active voices; the adjacent Shape guide shows the grain envelope.

![MoteField interface](docs/images/motefield.png)

**Current version: 0.2.6.** macOS universal VST3 and AU builds have been built, installed, and validated. Windows x64 build and installer automation is included; Windows host compatibility is still being verified. This is a development release.

## Included effects

Select an effect by turning the mode dial or clicking its name. The pointer aligns with the selected effect. A-D selects its variation, changing playback patterns, pitch behavior, or delay arrangements depending on the mode.

| Family | Effect | What it does |
| --- | --- | --- |
| Micro Loops | **Bloom** | Layers overlapping phrases at related playback speeds. Variations introduce upper octaves, lower octaves, or a cycling speed pattern for swelling, harmonically layered loops. |
| Micro Loops | **Chain** | Reads recent audio in a repeating slice sequence to build rhythmic phrases. Variations add half-speed and octave-related playback; D also introduces stepped amplitude quantization for a rougher texture. |
| Micro Loops | **Slide** | Bends the playback speed within each loop, creating rising, falling, or alternating pitch sweeps. Repeats extends the phrases and their overlap. |
| Granules | **Veil** | Scatters many small grains across recent audio to create a diffuse cloud. Variations range from slight detuning to wider pitch motion and octave layers. Shape also changes grain length. |
| Granules | **Orbit** | Overlaps long grains from different points in the recent capture, building sustained, slowly evolving textures. Variations include half-speed playback and pitch glides. |
| Granules | **Pluck** | Uses detected note attacks to choose where grains begin, keeping fragments connected to the articulation of the source. Variations add slight detuning or occasional octave-up voices. |
| Glitch | **Chop** | Replays short slices as rapid rhythmic cuts, with note attacks able to retrigger the pattern. Variations add changing playback speeds, octave steps, and a quantized texture. |
| Glitch | **Break** | Produces intermittent bursts with gaps between them. Activity changes the likelihood and rate of events; detected attacks can trigger fresh fragments. Variations change pitch and playback speed. |
| Glitch | **Ladder** | Steps individual fragments through repeating pitch-ratio sequences, using recent note attacks when available. Variations offer ascending, descending, octave, or reordered patterns; D adds amplitude quantization. |
| Multi Delay | **Grid** | Creates tempo-related stereo tap patterns. Activity changes the tap count, Repeats controls feedback, and A-D selects the timing arrangement. Grid uses a delay-tap view because Shape does not affect this mode. |
| Multi Delay | **Smear** | Blends multi-tap delay with granular playback. Shape changes modulation, smoothing, cross-feedback, and grain envelopes, turning distinct repeats into a more diffuse texture. |

### Shared processing

| Feature | What it does |
| --- | --- |
| **Filter / Resonance** | Rolls off high frequencies and emphasizes the region around the cutoff. |
| **Drift / Drift Rate** | Adds pitch modulation, with separate depth and speed controls in Details. |
| **Space** | Adds the selected reverb character to the effect signal. |
| **Bright reverb** | The least-damped, shortest-feedback reverb character. |
| **Dark reverb** | Stronger high-frequency damping for a darker tail. |
| **Hall reverb** | Longer feedback and greater stereo width for a larger space. |
| **Infinite reverb** | The longest-feedback and widest reverb character. Despite the name, its tail decays; use Hold to sustain captured material. |
| **FX Reverse** | Reverses effect playback. It is independent of the phrase looper's reverse control. |
| **Hold** | Stops replacing the recent capture while playback continues. In Grid and Smear, it captures and repeats the recent delay phrase. |
| **Mix / Output** | Blends dry and processed audio, then trims the effect output level. |
| **Bypass** | Smoothly returns to the unprocessed input while the engine and phrase transport continue running. |

## Main controls

| Control | Role |
| --- | --- |
| **Activity** | Changes event density, overlapping voice count, or delay tap count, depending on the mode. |
| **Shape** | Changes the volume contour of newly created grains. Also changes grain length in Veil and delay character in Smear. Not used by Grid. |
| **Filter** | Sets the low-pass cutoff. |
| **Mix** | Sets the dry/effect balance. |
| **Time** | Chooses a rhythmic subdivision when synced, or internal tempo in manual mode. |
| **Repeats** | Extends grain duration and feedback, or delay feedback, depending on the mode. |
| **Space** | Sets the reverb amount. |
| **Loop Level** | Sets the recorded phrase's contribution. |

Drag a knob vertically, scroll to adjust it, or double-click to reset it. Hold Shift while dragging for fine adjustment. Values appear on the knob; percentages use whole numbers.

**SUBDIV** follows host tempo when available, with an internal fallback. Switch to **TEMPO** for manual timing. Tap two or more times to set a manual tempo between 40 and 240 BPM. **Details** reveals modulation, resonance, output trim, reverb character, subdivision, loop speed, and Less Motion.

The window opens at 1000 x 645, or smaller to fit the screen. It is resizable, and opening Details stays within the existing window height.

## Phrase looper

Record up to 60 seconds and layer it independently of the selected effect.

| Control | Action |
| --- | --- |
| **Rec** | Start recording a phrase; press again to close the recording and play it. |
| **Play** | Finish recording or overdubbing, or resume a stopped phrase. |
| **Dub** | Start or finish an overdub pass. |
| **Stop** | Stop playback while retaining the phrase. |
| **Undo** | Remove the newest overdub pass, including a partial pass. |
| **Erase** | Clear the phrase and all overdubs. |
| **Pre / Post** | Place the looper before or after the effects. |
| **Loop Reverse** | Reverse phrase playback independently of FX Reverse. |
| **Loop Speed** | Play at half, normal, or double speed. Available in Details. |

**Recorded loop audio is temporary.** Render or record the output to a DAW track before closing the project. Sound parameters persist in sessions; phrase audio does not survive plug-in reloads or sample-rate changes.

## Presets and automation

The preset menu has **Factory** and **User** submenus. Choose from 32 factory starting points covering all eleven modes, or use **Save** to name the current sound. Replacing an existing user preset requires confirmation. An asterisk marks changes from the selected preset, and its name is retained with the DAW session.

User presets are portable `.motefield` files stored in:

- macOS: `~/Library/Application Support/Rango Labs/MoteField/Presets`
- Windows: `%APPDATA%/Rango Labs/MoteField/Presets`

Use **Refresh user presets** after copying files into that folder. User presets save 21 sound, timing, and routing parameters. They leave Hold, Bypass, transport commands, and recorded phrase audio untouched. Factory presets retain the current timing/sync and looper configuration.

All **29 exposed audio parameters** support host automation, including Mode, Variation, Hold, Bypass, and six looper command triggers. UI edits notify the host, allowing automation recording where the DAW supports it. Tap writes Tempo and Host Sync; display preferences are not audio parameters.

Looper triggers execute on each **0-to-1 or 1-to-0 transition**. Alternate values for repeated commands; a held value does not retrigger. Commands apply at the next processing block, and repeated edges of the same command within a block coalesce. Avoid simultaneous conflicting transport commands. DAW-specific automation behavior still needs host testing.

## Audio-reactive display

Bass energy changes the main body's pressure and stretch, mids alter its contour, and treble adds localized surface tension. Active voices form persistent groups whose position and shape follow grain phase, pan, envelope, playback rate, and audio samples. Groups merge through a shared surface instead of covering one another. The material and logo settle when the signal stops.

The Shape guide uses the same envelope mapping as the audio engine. Moving markers show active grains; existing grains can retain their earlier envelope until they finish. Grid shows its actual delay taps instead. **Less Motion** reduces spatial movement and holds the logo still.

The display is a visual interpretation of the sound. Rendering runs on the UI thread using bounded buffers; audio processing publishes snapshots without waiting for painting.

## Build and install

The project uses C++20, CMake 3.22+, Git, and a pinned JUCE dependency fetched during configuration. Mono-to-mono and stereo-to-stereo processing are supported.

### macOS

Install full Xcode and select it as the active developer directory, then run:

```sh
bash scripts/build-macos.sh
# Close your DAW before replacing a loaded plug-in.
bash scripts/install-macos.sh
```

The script builds universal Apple Silicon/Intel VST3 and AU bundles, runs the DSP tests, and stages locally signed development bundles in `dist/macos`. Installation backs up previous copies and uses:

- `~/Library/Audio/Plug-Ins/VST3/MoteField.vst3`
- `~/Library/Audio/Plug-Ins/Components/MoteField.component`

Reopen your DAW, enable VST3/AU scanning, and rescan if needed. Load MoteField on an audio track receiving a signal. Start with **First Light**, raise Mix, and try Hold on a sustained chord.

Create a development installer with `bash scripts/package-macos.sh --unsigned`. See [release preparation](RELEASE.md) for signed distribution.

### Windows

Use Visual Studio with Desktop development with C++, CMake, and Git. From a developer terminal:

```powershell
./scripts/build-windows.ps1
./scripts/package-windows.ps1 -Unsigned
```

Packaging also requires PowerShell 7 and Inno Setup 6.3 or newer. The installer targets `C:\Program Files\Common Files\VST3\MoteField.vst3` and includes an uninstaller. Windows x64 builds and installers are configured in [GitHub Actions](.github/workflows/build.yml); native Windows DAW testing remains pending.

### Linux

Install the JUCE platform dependencies for X11, ALSA, OpenGL, FreeType, and fontconfig, plus CMake and a C++20 compiler:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Artifacts are under `build/MoteField_artefacts/Release`. Linux VST3 bundles can be installed in `~/.vst3/`. Platform binaries are not interchangeable.

## Tests and release status

The DSP suite covers all mode/variation combinations, buffer-size determinism, Hold, layered overdubbing and undo, output telemetry, frequency-band response, oversized buffers, and allocation-free processing in the instrumented callback test.

An optional native editor harness checks presets, automation notifications, mode-pointer alignment, value formatting, window sizing, material response, and silence settling:

```sh
cmake -S . -B build -DMOTEFIELD_BUILD_PREVIEW=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target MoteFieldPreview --config Release --parallel 2
# Executable location depends on the CMake generator.
./build/MoteFieldPreview /absolute/path/to/preview-output --stills-only
```

See [verification](VERIFICATION.md) for completed checks and remaining platform testing. High feedback and stacked loops can exceed full scale; Output controls the effect path and Loop Level controls the phrase contribution. Dedicated MIDI CC learn and phrase-audio persistence are not implemented.

## License

Original source is available under the [MIT License](LICENSE). JUCE and its bundled dependencies retain their own licensing terms. Open Sauce Sans font notices are included in [Assets/OFL.txt](Assets/OFL.txt).
