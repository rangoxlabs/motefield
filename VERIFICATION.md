# Verification

## Version 0.3.0

Universal macOS VST3, AU, standalone, and native editor harness builds completed. VST3 and AU are installed locally and report 0.3.0. The installed VST3 executable matches the staged binary by SHA-256. Previous installed copies were backed up before replacement.

### DSP

The full suite passed in 47.01 seconds on the development Mac. It covers:

- All 11 modes and four variations, finite output, buffer-size determinism, oversized processing, mono bypass, and allocation-free engine callbacks.
- Granular and delay Hold, layered overdubbing, partial-pass undo, repeated undo, and queued transport commands.
- Exact quarter-note loop boundaries from a fractional host beat position, pending-command telemetry, burst replacement, and cancellation of a stale quantized command when Burst starts.
- Phrase fade-out, release of Hold during trails bypass, and stopping a phrase while tails decay.
- Loop snapshot/restore, concurrent snapshots during playback, sample-rate conversion, and capture of recent final output.
- Audible scan/pitch/stretch/split gestures, viscosity response time, cohesion, tension, sidechain compression, reproducible random patterns, rhythm/pitch mutation, and scale constraints.
- Output telemetry, independent frequency-band response, antiphase stereo energy, and silence decay.

The performance/material subset also passed with AddressSanitizer and UndefinedBehaviorSanitizer. Leak detection was disabled for that run. Concurrent snapshot tests are functional coverage, not a ThreadSanitizer result.

### Processor and native editor

- Fluid dragging and double-click reset send balanced host automation gestures. MIDI CC learn begins recording at the event's sample offset; releasing a transport CC does not retrigger it.
- Stereo WAV export has the expected sample count and channel count.
- Recorded phrase audio round-trips through DAW state and user preset files, including state restored before audio preparation.
- The enabled mono sidechain bus reaches the magnet follower; retrospective capture replaces the phrase; malformed audio archives are rejected.
- All 58 exposed parameters are automatable and UI changes provide host gesture notifications. Existing parameter IDs and stepped loop-speed choices are retained.
- 32 factory presets cover all modes. User-preset tests cover overwrite protection, malformed-file rejection, preset identity, and sound-setting round trips.
- Initial window dimensions, Details sizing, mode-pointer alignment, preset text, and whole-number knob percentages pass.
- The Performance panel opens and closes within the editor, and screenshots of all three pages were inspected. Controls beyond the viewport are reached by scrolling.
- Frequency-dependent material deformation, short snapshot-gap stability, and settling to the resting surface pass.

Apple's `auval -v aufx MtFd Rngo` succeeded, including parameter scheduling tests. It reports a warning that MIDI input is implemented on an `aufx` component rather than `aumf`. The original AU identity is intentionally retained for existing projects; use VST3 for host-routed MIDI where supported. Direct MIDI routing into this AU is not qualified in Logic.

### Private Mac test kit

The ZIP contains the two universal plug-ins, `Install.command`, payload checksums, and plain-text instructions. Installation is per-user, with backup of older copies and no administrator password. The installer clears quarantine only on its staged MoteField bundles after file-integrity checks.

The 0.2.6 kit was extracted, marked with simulated browser quarantine, installed into an isolated test root, and reinstalled to exercise backup behavior. Installed bundle signatures verified and quarantine was absent from the installed copies. This is not a clean-machine test. The 0.3.0 kit separately passed quarantined installation, signature checks, and rejection of a deliberately modified payload before any destination files were written.

## Platform and release limits

- The previous 0.2.6 GitHub Actions run passed on macOS and Windows. The expanded 0.3.0 Windows build and native DAW testing remain to be verified.
- Live controller routing, automation recording/playback, MIDI preset changes, and external WAV dragging need hands-on checks in each supported DAW.
- Continuous speed is varispeed: it changes pitch. Tempo changes after recording do not automatically time-stretch a phrase to the new tempo.
- Scales constrain grain transposition using a manually supplied source note; this is not polyphonic pitch correction.
- Snapshots taken during recording, overdubbing, or transport changes capture live material. Stop editing the phrase first when an exact finalized take is required.
- High feedback and stacked loops can exceed full scale. There is no output limiter; 24-bit WAV export can clip audio above full scale.
- Clean-machine macOS installation, Intel host execution, sustained multi-instance performance, and signed/notarized release packaging remain pending.

The fluid is an interactive visual interpretation of the engine, not a physical magnetic-fluid simulation. Native editor checks exercise callbacks and attachments; they do not replace testing in a real DAW.
