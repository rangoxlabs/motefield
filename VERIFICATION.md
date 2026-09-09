# 0.3.5 verification

Width now runs before POST looper capture. Its setting is printed into new POST recordings, so changing Width afterward cannot reshape the stored playback; the WAV export contains that same print. Existing loops remain unchanged. PRE capture remains unprocessed and plays through the live effects as designed.

Above 100%, the control eases into a maximum side gain of 1.10 instead of 2.0. Narrowing and 100% unity are retained. No delays or phase rotation are introduced; the mono sum is unchanged. This reduces added phase sensitivity, but cannot guarantee positive correlation for source material or effects already near or below zero correlation.

Local DSP and click suites passed (62.16 s / 8.13 s), including the 10% side-gain ceiling, unchanged mono sum, a positively correlated wide test signal, POST width printing, identical existing POST playback at 0/100/200%, unchanged export data and preserved PRE routing. Platform package results follow below. Preset initialization is not implemented in this release.

# 0.3.4 verification

Current release candidate adds stereo Width, Wet Solo, Level Match and stronger audio-driven reactor deformation. The optional Magnet sidechain now accepts disabled, mono or stereo layouts independently of the matching mono/stereo main buses.

- DSP and click suites pass locally, including new checks for 0/100/200% mid/side width, mono transparency, fully wet equivalence, gain matching, silent-input behavior, bypass and unchanged POST recording.
- Existing parameter IDs/order are retained; three automatable parameters are appended (61 total). Old sessions and presets default to Width 100%, with monitoring off for old sessions. Wet Solo and Level Match are session controls and are excluded from sound presets.
- Direct probing of the installed 0.3.3 AU reproduced rejection of a stereo auxiliary input (-10868), while its stereo main input and auval passed. The candidate accepts both auxiliary channel formats. Actual Logic Pro stereo playback remains to be confirmed; Logic Pro is unavailable on this Mac.
- Native integration/UI tests pass: 61 automatable parameters, monitor button attachments, Width preset recall, legacy session defaults, all main/aux bus combinations, no jumps beyond ongoing motion during snapshot gaps, frequency-driven deformation and silence settling. Native screenshots were inspected.
- Universal macOS Release build and DSP/click tests pass (58.19 s / 8.09 s on the initial test run). Installed AU and VST3 report 0.3.4 and match staged binary hashes. AU validation passes; direct Audio Unit probing accepts and initializes stereo main input with both mono and stereo auxiliary inputs (all status 0).
- Mac friend ZIP SHA-256: `4de3d20a43812382b367cd4b0c76c14182d0ba92dfbc18fc145026461824dccb`. Extracted ZIP passed clean install, reinstall backup, installed-binary identity and rejection of corrupt payloads before installation.
- [Windows run 34248125689](https://github.com/rangoxlabs/motefield/actions/runs/34248125689) passed at `d02e28d`: native x64 build, DSP/click tests (21.79 s / 2.70 s), ZIP clean install/replacement/hash/corruption checks, and Inno installer/uninstaller checks.
- Downloaded Windows ZIP and installer checksums verified. ZIP CRC, payload manifest, 0.3.4 module version and x64 PE architecture verified. Windows ZIP SHA-256: `9972fefa019f24bbd3a739cc67d26faab06b7d324f9729bbe16496549be0d476`.
- Actual Logic Pro stereo playback, Intel Mac DAW playback, and Windows DAW playback remain host-specific validation items.

## Earlier verification records

# Verification

## Version 0.3.3

Restores the voice-driven implicit fluid surface in place of the incorrectly retained logo silhouette, preserving the approved enclosure and pearl material. Adds RANDOM beside the preset picker to generate sound settings while retaining recorded audio, transport, timing/sync and output gain. Universal VST3 and AU are installed and match the staged 0.3.3 binaries.

- DSP and click suites passed in 50.52 and 7.84 seconds.
- Native checks include a round resting fluid volume, changed geometry when voice positions move with unchanged meters, independent audio-band response, snapshot stability and silence settling.
- RANDOM checks passed for host gestures, preservation of non-randomized parameters and loop audio, repeatable seeds, different generated sounds, and user preset save/recall. Existing full UI/integration tests passed.
- AU validation passed with the existing MIDI-on-aufx warning.
- Extracted friend kit passed simulated quarantine installation, reinstall backup, universal signatures and versions, and rejection of a corrupted payload. Clean-machine and Intel DAW execution are still pending.
- ZIP SHA-256: `d1823b8b8a9205f38dfedf9452ce44b7de588bc2587d825befbd83a2e5553692`.


### Windows 0.3.3

[Windows build run 34089684372](https://github.com/rangoxlabs/motefield/actions/runs/34089684372) passed at commit `3d371c8`.

- Native x64 build passed; DSP and click tests passed in 19.26 and 2.75 seconds.
- Extracted direct-install ZIP passed fresh installation, replacement backup, installed-binary hash comparison, and rejection of a deliberately corrupted payload before installation.
- The unsigned Inno installer installed version 0.3.3 with a matching binary, and its uninstaller passed.
- Downloaded ZIP checksum and CRC passed locally; the bundle contains a Windows x64 PE binary and reports version 0.3.3.
- ZIP SHA-256: `ab434e8e915b7bc5019df790684865ca969f2398c4e993ed3eaab2865ccc66a0`.
- Native Windows DAW playback and Windows ARM remain unqualified.

### Full-phrase promos (revision 4)

Rejected revision 3 exports and archives were deleted. The original supplied Closure arp and Hope piano WAVs remain unchanged by SHA-256. The earlier one-bar slicing schedule was replaced with complete-file passes and sample-accurate chapter boundaries. Native source counters and full-length waveform comparisons verify every source sample plays once per pass, including the endpoints. Video frame rounding no longer determines audio cut points.

Four preset tours cover six presets per source, with three presets and a complete dry reference in each video. Each processed pass includes two seconds for the effects to decay. All 32 factory presets were rendered against both full sources and measured for finite output, headroom, transient steps, stereo energy, and contrast; selections favor octave-compatible layers and distinct rhythmic/textural roles. These measurements are not subjective listening approval.

The replacement looper performance records the complete piano phrase in PRE-FX at Mix 0%, then turns off the original input. It plays the recording through Soft Focus and Moon Pool and performs a gradual filter/space/shape swell. The captured file contains exactly 1,012,748 samples at 48 kHz, matching the resampled source length; its interior sample error against the clean source at capture gain is below 1.2e-7. Loop playback stops only after the final full pass, followed by a two-second tail.

All five videos are 1920 x 1080 at 30 fps. Arp tours run 64.24 seconds each, piano tours 90.90 seconds each, and the PRE-FX performance 86.40 seconds. Encoded true peaks are -3.0 dBFS for arp and -3.6 dBFS for piano and looper. Each video is below 12 MB; the verified five-video ZIP is 38.9 MB. Native Mix/transport checks and the full-file checks in `scripts/package-promos.py` passed. Captures are offline native processor/editor renders with editorial labels.


## Version 0.3.2

The approved clear Acid interface was built as universal macOS VST3, AU, standalone, and native preview binaries. VST3 and AU are installed locally, report 0.3.2, and match the staged release executables byte-for-byte. Previous bundles were backed up before replacement.

- DSP tests passed in 51.30 seconds; click-regression tests passed in 8.60 seconds.
- Native integration/UI checks passed for modes, selector alignment, variations, presets, automation attachments, looper audio recall/export, MIDI learn, sidechain routing, appearance persistence, resizing, and material response/settling.
- Apple AU validation passed. The existing MIDI-on-aufx warning remains as described below.
- The 33,523,213-byte private Mac ZIP was extracted and tested with simulated download quarantine. Fresh installation, reinstallation backup, signature/architecture/version checks, staged-binary equality, and rejection of a modified payload all passed in an isolated installation root. This is not a clean-machine or Intel DAW execution test.
- ZIP SHA-256: `31b0139e6e61834fc901478d96588c538f0121167ec7a1fd3be5bd66b7835812`. This kit is locally signed for private testing and is not notarized.


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

- The 0.3.0 GitHub Actions [build run](https://github.com/rangoxlabs/motefield/actions/runs/34070320230) passed on macOS and Windows at commit 8708259. Both platforms compiled, passed the DSP tests, and produced installer artifacts; macOS also produced the direct-install test ZIP. Native Windows DAW playback remains to be verified.
- Live controller routing, automation recording/playback, MIDI preset changes, and external WAV dragging need hands-on checks in each supported DAW.
- Continuous speed is varispeed: it changes pitch. Tempo changes after recording do not automatically time-stretch a phrase to the new tempo.
- Scales constrain grain transposition using a manually supplied source note; this is not polyphonic pitch correction.
- Snapshots taken during recording, overdubbing, or transport changes capture live material. Stop editing the phrase first when an exact finalized take is required.
- High feedback and stacked loops can exceed full scale. There is no output limiter; 24-bit WAV export can clip audio above full scale.
- Clean-machine macOS installation, Intel host execution, sustained multi-instance performance, and signed/notarized release packaging remain pending.

The fluid is an interactive visual interpretation of the engine, not a physical magnetic-fluid simulation. Native editor checks exercise callbacks and attachments; they do not replace testing in a real DAW.
