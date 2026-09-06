# Verification

## Version 0.2.6

macOS universal VST3, AU, and standalone targets compiled with Xcode 26.6. The VST3 and AU were installed locally, both reported 0.2.6, VST3 signature verification passed, and `auval -v aufx MtFd Rngo` succeeded.

### DSP checks

The suite passed in 32.33 seconds on the development Mac. Coverage includes:

- All 11 modes and four variation selections, with finite, non-silent output under the test stimulus.
- Buffer-size determinism and oversized buffer chunking.
- Granular Hold and captured delay Hold.
- Layered overdubbing, partial overdub removal, repeated undo, and ordered transport commands.
- Output waveform telemetry compared with the actual host output, in mono/stereo at 48/96 kHz.
- Visual frequency-band separation at 48/96 kHz, antiphase stereo energy, and decay after silence.
- In-place mono bypass and no heap allocations in the instrumented oversized callback test.

These checks do not establish an output limiter or a ceiling for every feedback and looper combination.

### Native editor and processor checks

- Compact initial window, Details sizing, and visible controls within the editor bounds.
- All eleven mode-dial values, label/pointer alignment, and nonoverlapping label hit areas.
- Whole-number percentage formatting and correct Save preset menu text.
- All mode and variation selectors, synced/manual Time, Hold/Bypass, looper controls, and FX Reverse.
- 32 factory presets covering all eleven modes; user-file save/load, overwrite protection, malformed-file rejection, and preset identity across session recall.
- All 29 host parameters, complete UI gesture notifications, six looper triggers, held-trigger non-repetition, and suppression of transport commands during state recall.
- Distinct bass/mid/treble material shapes, stability across short gaps between audio snapshots, and return to the resting surface after silence.

Compiled editor, compact window, Details, and audio-driven material screenshots were inspected. The README image is a native render of the current interface.

### Earlier checks

Linux x86_64 compiled an earlier preview. The DSP regression suite was also exercised under AddressSanitizer and UndefinedBehaviorSanitizer; leak detection was disabled in that environment. This is not a current Linux release qualification or a leak-check result.

## Remaining validation

- Native Windows compilation, installer upgrade/uninstall, and DAW scanning/playback.
- Automation recording/playback and session recall in each supported DAW.
- Clean-machine macOS installation and Intel-host execution.
- Sustained rendering performance with multiple open plug-in instances.
- Signed and notarized release packaging.

The editor harness exercises component callbacks and parameter attachments; it does not replace hands-on host testing. Phrase-audio persistence and dedicated MIDI CC learn are not implemented.
