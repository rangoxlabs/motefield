# 0.4.0 development beta verification

On `feature/musical-response`; main and the locally installed plug-ins remain 0.3.7. This beta directly updates the engine sound, while retaining all 65 parameter IDs/order.

Universal macOS AU/VST3 and native preview compilation passed. The final DSP and click suites passed (124.43 s / 9.36 s). Coverage includes all 44 variations and pairwise distinction within each mode, quiet opposite-polarity input, retained-source capture/recall/resampling, concurrent source snapshots/restores, invalid source rejection, and the existing loop timing, export, width, monitoring, pattern and buffer checks. Additional maximum-activity/repeats/space cases passed at 8, 44.1, 96 and 192 kHz across Bloom, Veil and Smear, including changing variations and tail filtering; the instrumented processing callbacks allocated no heap memory.

AddressSanitizer and UndefinedBehaviorSanitizer passed the retained-source, rate-boundary, recording, width, monitoring, performance and material subset. Leak detection was disabled. Concurrent snapshot checks are functional and sanitizer coverage, not a ThreadSanitizer result.

Native integration passed for kept-source audio in sessions loaded before preparation and in user preset files, corrupt archives, the 37 factory presets, all 65 automatable parameters, initialization, tuning, appearance, material response and layout. Screenshots were inspected. The main layout is retained; Perform now names the existing pattern control **Keep phrase**, explains first-sound arming and audio persistence, and the reactor can display **KEPT**.

The Mac ZIP passed CRC, payload signatures, universal architecture/version checks, clean installation and replacement with backup, exact installed-binary comparison, quarantine clearance on its staged bundles and corrupt-payload rejection before writing destinations. These checks used an isolated installation root. Mac ZIP SHA-256: `4e7217ba4d5217990c98593b1ea0125bb4a30e79a9cf0a87236a068f549df0f7`. The real installed 0.3.7 plug-ins were not replaced; AU validation of 0.4.0 in the installed environment and real DAW playback remain pending.

Windows CI and the response/audition receipt will be recorded below after completion. These tests establish software behavior and signal integrity, not subjective musical approval or equivalence to any hardware effect.

# 0.3.7 verification

Adds factory recall of tuning/material/pattern state, five tuned factory presets (37 total), an in-place tuning page, and next-beat/bar recording with optional one-bar count-in and fixed lengths. Four parameters are appended, preserving all 61 existing parameter IDs/order (65 total). Factory references remain 440 Hz; old sessions and presets default missing new controls without dropping stored transpose.

Universal macOS AU/VST3/standalone compilation passed. DSP and click suites passed (63.44 s / 8.35 s): exact four-bar recording after count-in, stopped-host arming/restart, 3/4, 1/4 and 1/8 meters, cancel, tempo changes, overdub finish, legacy beat scheduling, allocation safety, width print/export consistency, output monitoring and the existing 44 mode/variation checks. Native integration passed for tuning navigation/entry at 1000 and 1620 px, preset pitch reset, five new interval offsets, 440 Hz factory defaults, user tuning save/recall, all 65 host parameters and existing UI/material/initialization checks. Native screenshots inspected; a near-zero numerical display artifact was corrected before packaging.

Mac test ZIP passed CRC, clean install, reinstall backup, exact binary/version checks and corrupt-payload rejection in an isolated folder. SHA-256: `83aa176318f7549f92ac61be68d798cb38e61f84f953d85d6937899924e97793`. Final native integration also passed old-session and 0.3.6 user-preset migration. Installed per-user AU/VST3 versions are 0.3.7 and match the staged binaries exactly; both universal architectures and ad-hoc signatures passed, with no system-wide duplicates. Previous installed bundles are backed up under `~/Library/Application Support/Rango Labs/MoteField/Backups/install-jcwD6MfP`. AU validation passed with mono/stereo main-bus support.

GitHub [run 34430142804](https://github.com/rangoxlabs/motefield/actions/runs/34430142804) passed for macOS and Windows at `7a03af0`, including DSP/click suites, Windows ZIP install/replacement/corruption tests and Windows installer/uninstaller checks. Downloaded Windows ZIP/EXE checksums, ZIP CRC, complete payload manifest, x64 PE and 0.3.7 module version verified. Windows ZIP SHA-256: `742042c12f62d62ab1a05b0a5d4f91470378f9ff53efff246c9f6b528bb04159`.

The five added factory presets also rendered finite, non-silent output at 100% wet from the supplied 133 BPM arpeggio (input gain 0.4, full phrase plus two-second tails). Peaks ranged from -19.9 to -13.7 dBFS. This is a signal-safety check on one source, not subjective listening approval. Logic stereo playback and listening on testers' machines still require host verification; automated checks are not a substitute for that listening pass.

# 0.3.6 verification

Adds Initialize sound to the preset menu with a simple variation-A starting sound for each of the 11 modes. Keeps the selected mode, timing/subdivision, output/monitoring, looper controls and recorded audio. A one-step Undo initialization restores the previous sound and preset identity without reverting later changes to protected controls. No parameter IDs or ordering changed (61 host parameters).

Universal macOS AU/VST3/standalone and native preview compilation passed. Native checks passed for initialization through the preset menu in all 11 modes, balanced host gestures, retained loop audio/transport and protected controls, undo value/identity restoration, saved-file preservation, initialized-preset save/recall, and invalidation of stale undo after saving, loading a factory preset or restoring a session. Existing processor integration, appearance, preset/automation, material response and UI checks also passed. Mac ZIP verification passed: universal architectures and signatures, ZIP CRC, 0.3.6 bundle versions, clean installation, reinstall backups, payload identity and corrupt-payload rejection in an isolated folder. Mac ZIP SHA-256: `518d9a49023a70f22e0435ece3a9f8873d12a6b9b97b7290853bfbd13f9cb098`. Windows [run 34303097849](https://github.com/rangoxlabs/motefield/actions/runs/34303097849) passed at `eff163a`: native x64 compilation, DSP/click tests, ZIP clean-install/replacement/corruption checks, and installer/uninstaller checks. Downloaded ZIP and installer checksums, ZIP CRC, payload manifest, 0.3.6 module version and x64 PE verified. Windows ZIP SHA-256: `ea26248c24aa89ba01bf78190ff3db7c32288e92c2f5bcc86cff2e27736cae9d`. The 0.3.6 per-user AU/VST3 were subsequently installed locally, matched against the staged binaries, and passed AU validation.

# 0.3.5 verification

Width now runs before POST looper capture. Its setting is printed into new POST recordings, so changing Width afterward cannot reshape the stored playback; the WAV export contains that same print. Existing loops remain unchanged. PRE capture remains unprocessed and plays through the live effects as designed.

Above 100%, the control eases into a maximum side gain of 1.10 instead of 2.0. Narrowing and 100% unity are retained. No delays or phase rotation are introduced; the mono sum is unchanged. This reduces added phase sensitivity, but cannot guarantee positive correlation for source material or effects already near or below zero correlation.

Local DSP and click suites passed (62.16 s / 8.13 s), including the 10% side-gain ceiling, unchanged mono sum, a positively correlated wide test signal, POST width printing, identical existing POST playback at 0/100/200%, unchanged export data and preserved PRE routing. The wide regression signal retained positive correlation (0.0101) at maximum Width.

Universal macOS AU/VST3/standalone compilation and native processor/UI integration checks passed. The Mac ZIP passed CRC, signatures, 0.3.5 version and payload identity checks, clean install, reinstall backup and corrupt-payload rejection in an isolated installation folder. Mac ZIP SHA-256: `f3a83ad7fe819a2f5ed58dc3ca7cdaf206c62d7348c32416ff6fe81131f319f2`. Windows [run 34301570371](https://github.com/rangoxlabs/motefield/actions/runs/34301570371) passed at `496fe69`: native x64 build, DSP/click tests (23.09 s / 2.71 s), direct-install ZIP checks, and installer/uninstaller checks. Downloaded ZIP CRC, payload manifest, x64 PE architecture, version and ZIP/installer checksums verified. Windows ZIP SHA-256: `955108975dc93cf29baee34f3431a548403de79ffaa2af2dc71eb0ae7033b57a`. The local installed plug-ins were not replaced during this pass. Preset initialization is not implemented in this release.

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
