# Release preparation

## 0.3.13 release candidate — 2026-10-07

The original AAX/DMG work was merged to main in PR #2. Automatic-installer follow-up is on `release/macos-installer-0313`; no public binary release has been published. This candidate includes the previously tested 0.3.12 pitch lock, reverb contrast, reactor feedback and 43 presets, plus AAX Native and distribution tooling. The separate browser appearance prototype is not included. Distribution is by direct download; no store submission is underway.

| Track | Completed | Remaining |
| --- | --- | --- |
| macOS AU/VST3 | Universal build, Developer ID Application signing, hardened runtime, timestamps, Apple notarization, stapling and Gatekeeper assessment of the automatic PKG and manual-copy DMG. Installed AU validation passes on arm64 and x86_64. | Clean-machine downloaded installation and DAW listening/automation checks. Company signing identity requires organization membership if desired for launch. |
| AAX Native | Universal macOS and Windows x64 builds; development installers use standard Avid folders. Windows installer/uninstaller checks pass. Commercial/PACE onboarding request sent to Avid. | Avid commercial agreement and publisher/PACE access; Pro Tools Developer host tests; final PACE-signed retail Pro Tools tests, including save/reopen. |
| Windows signing | Unsigned VST3/AAX build and installer checks pass. Signing scripts support a certificate store or Artifact Signing. | Account setup resumed; Azure enrollment, organization validation and signing remain pending. |

The preferred automatic installer candidate is `dist/installers/MoteField-0.3.13-macOS-universal.pkg`, SHA256 `ff2eeb62e0d4d924cdf4db4f0bcd2e2bf9cee372276a6e2eff22c99f2f984f25` (33,733,443 bytes). Apple accepted submission `937995d2-5322-481b-b503-a989753a73c0` with no issues. The final PKG is signed, notarized, stapled and accepted by Gatekeeper. Expanded payload checks confirm universal signed AU/VST3 0.3.13, automatic system destinations, dependency notices and both installer pages. Native Installer shows a valid certificate. The actual local installation succeeded: the receipt reports 0.3.13, all 23 payload files match, signatures verify, and the system AU passes arm64 and Rosetta x86_64 validation. Clean-machine downloaded installation and DAW listening/automation checks remain pending.

The earlier manual-copy DMG remains at `dist/installers/MoteField-0.3.13-macOS-universal.dmg`, SHA256 `ff356c2ed7e2c2a4f697d06f3a57aaa0b2d99c61363cdc742b277fc2614db04b`. Its Apple submission is `025ecf79-09a1-4aa0-a0cb-c286ca657dc3`. Both artifacts are private candidates, not published.

The installer and plug-in display Rango Labs branding. The existing individual Apple team's certificates identify the account holder as the verified signer. Company-name signing requires an organization membership and corresponding company certificates; changing the installer title does not change that identity. Apple allows founders/cofounders to request conversion and requires the organization's D-U-N-S details, with possible business-document verification. [Apple membership conversion](https://developer.apple.com/help/account/membership/updating-your-account-information/). The account holder is handling the organization conversion inquiry with Apple; approval is not yet confirmed.

[CI run 37563012810](https://github.com/rangoxlabs/motefield/actions/runs/37563012810) passed macOS and Windows at `885f478`. Its installers are development artifacts, not the separately notarized PKG/DMG. See [VERIFICATION.md](VERIFICATION.md) for evidence and outstanding host checks.

## AAX onboarding and validation

The approved `PACE AAX Code Signing Tools Request — Rango Labs` email was sent to `audiosdk@avid.com`. It supplies the company, administrator, contact, website, plug-in, Avid and iLok information, and asks about account ownership, the commercial agreement, signing access and any independent-distribution certification requirement. The existing Pro Tools NFR is a testing entitlement, not proof of commercial AAX distribution approval. [Avid AAX guidance](https://developer.avid.com/aax/), [PACE onboarding](https://paceap.com/getting-started-with-aax-code-signing-for-pro-tools-plugins/).

Avid approves publishers and refers them to PACE. The AAX SDK licensing documentation calls for a commercial agreement for independent sales. The old self-service agreement URL redirects; a current agreement is awaiting Avid's response. Public Alliance Partner/Marketplace certification guidance does not establish a universal exam prerequisite for independently distributed AAX. Do not describe that requirement as either confirmed or waived. Physical-iLok signing and PACE's cloud signing service are separate from iLok Cloud activation for running Pro Tools.

Build AAX with `MOTEFIELD_BUILD_AAX=ON bash scripts/build-macos.sh` or `./scripts/build-windows.ps1 -AAX`. JUCE's bundled SDK is the default; optionally set `MOTEFIELD_AAX_SDK_PATH` on Mac or pass `-AaxSdkPath` on Windows. AudioSuite and multi-mono are disabled pending dedicated validation. Use Pro Tools Developer for unsigned AAX; saving is disabled there. Final session-recall testing needs the signed plug-in in retail Pro Tools.

Before distribution, verify mono/stereo main buses and optional sidechain routing; audio processing and latency; host/internal bypass; transport, tempo and sample-rate changes; MIDI CC/program routing; automation gestures/record/playback; all factory programs; copied instances; saved sessions and recorded-loop recall. Actual Pro Tools instantiation/audio testing is still pending. Build success does not establish host compatibility.

Development packages:

```sh
bash scripts/package-macos.sh --unsigned --include-aax
```

```powershell
./scripts/package-windows.ps1 -Unsigned -AAX
```

These place `.aaxplugin` in `/Library/Application Support/Avid/Audio/Plug-Ins` or `C:\Program Files\Common Files\Avid\Audio\Plug-Ins`. Follow the supplied PACE workflow to finish OS signing and Eden wrapping before packaging. Release packaging requires a final PACE-signed AAX bundle and runs signature verification without modifying it:

```sh
export MOTEFIELD_SIGNED_AAX_PATH='/absolute/path/to/signed/MoteField.aaxplugin'
# Also set the Apple identities and notarization profile below.
bash scripts/package-macos.sh --release --include-aax
```

On Windows, combine `-AAX -SignedAaxPath 'C:\path\to\MoteField.aaxplugin'` with the selected signing provider. `PACE_WRAPTOOL` (Mac) or `-PaceWrapTool` (Windows) can select the tool. PACE signing paths remain unexecuted until access is granted; confirm against the supplied tool version.

## macOS packaging

The signed, notarized DMG needs a Developer ID Application identity and a working `notarytool` keychain profile:

```sh
bash scripts/build-macos.sh
export DEVELOPER_ID_APPLICATION='Developer ID Application: YOUR LEGAL ENTITY (TEAMID)'
export NOTARY_PROFILE='your-notary-profile'
bash scripts/package-macos-dmg.sh
```

The script copies AU/VST3, adds installation instructions and dependency notices, signs with hardened runtime and timestamps, signs the DMG, requires accepted notarization, staples the ticket, and verifies Gatekeeper acceptance before moving it to `dist/installers`. Follow `INSTALL.txt` in the DMG for user-Library installation; quit the DAW first and back up existing bundles. User presets/session audio remain separate. The DMG does not contain AAX.

For an all-users PKG, also configure Developer ID Installer:

```sh
export DEVELOPER_ID_INSTALLER='Developer ID Installer: YOUR LEGAL ENTITY (TEAMID)'
bash scripts/package-macos.sh --release
```

The PKG installs AU/VST3 under `/Library/Audio/Plug-Ins`, and optional AAX under Avid's folder. A user-Library copy can shadow the system copy; use one installation location when testing upgrades. `--unsigned` deliberately creates a separate development artifact. Keep private keys, credentials and identity-specific setup outside the repository. [Apple Developer ID guidance](https://developer.apple.com/developer-id/).

## Windows packaging — signing onboarding deferred

Use Visual Studio with Desktop development with C++, CMake 3.22+, Git, PowerShell 7 and Inno Setup 6.3 or newer:

```powershell
./scripts/build-windows.ps1
./scripts/package-windows.ps1 -Unsigned
```

The x64 installer places the VST3 in `C:\Program Files\Common Files\VST3` and includes an uninstaller. Signed packaging can use `-CertificateThumbprint 'YOUR_CERTIFICATE_THUMBPRINT'` or the pair `-ArtifactSigningDlib 'C:\path\Azure.CodeSigning.Dlib.dll' -ArtifactSigningMetadata 'C:\path\metadata.json'`. The checked-in metadata file is a template, not an account. The script signs and verifies the plug-in and installer and configures signing of the uninstaller. No Microsoft account or signing service has been completed in this release pass.

## Publication gates

Confirm the applicable JUCE license before commercial publication; included third-party notices do not establish license entitlement. Test downloaded installation, upgrades, uninstall and host scanning on clean supported systems. Check listening, bypass, automation recording/playback, presets, session recall and recorded-loop persistence in the intended DAWs. Apple Silicon, Intel Mac and Windows DAW playback are separate checks. Publish only exact verified artifacts.

[Build plug-ins](.github/workflows/build.yml) builds/tests universal macOS and Windows x64 including AAX and packages development artifacts. It does not create public releases or use release-signing credentials. An iOS edition needs separate app/AUv3 work.

## Historical beta records

## Windows 0.3.12 complete — 2026-09-21

GitHub Actions run [35660917470](https://github.com/rangoxlabs/motefield/actions/runs/35660917470) succeeded at commit 29f2508 on build/windows-0.3.12. Windows x64 compilation, DSP/click tests, direct ZIP clean install/reinstall/backup/corruption rejection, and EXE installer/uninstaller checks passed. Downloaded ZIP CRC, complete payload manifest, module version 0.3.12, x64 PE and published SHA256 sidecars verified locally. Mac/Windows build source parity verified.

- ZIP: dist/friend-test/windows-0.3.12/MoteField-0.3.12-Windows-Test.zip (8,401,595 bytes), SHA256 32bd9b3fe7c9651d94572fdf31e6f71fcee85b3c334234ad4f52e175e5c0c978.
- EXE: dist/installers/windows-0.3.12/MoteField-0.3.12-Windows-x64-UNSIGNED.exe (9,243,538 bytes), SHA256 9a79f4142591f9ce3cf73093e054bd4fd1c3596c74122557ec0f41b54d61f67f.

Unsigned beta distribution; not a signing/notarization release. Windows DAW listening and actual Ableton automation playback still need user validation. Main branch remains untouched; build branches contain the tested source.


## macOS 0.3.12 beta — 2026-09-21

Includes PITCH LOCK beside the reactor, explicit drag directions, effect-mode preset folders plus Reactor Explorations, and more distinct Bright/Dark/Hall/Infinite reverbs with a stronger upper Space range. Existing preset values and IDs remain compatible, but reverb-enabled presets sound different. Pitch lock saves with the session, suppresses pitch writes from reactor drag/reset, and leaves explicit tuning/host automation active.

Universal build and DSP/click suites passed (94.46s / 9.32s). Native full UI/preset checks and pitch-lock/gesture/session checks passed. Reverb checks cover style decorrelation and tails, Space contrast, finite levels and smoothed transitions (test peak step .00213839). Installed AU/VST3 version 0.3.12, both architectures, full staging/installed payload equality and strict ad-hoc signatures verified. Installed AU validation passed. Previous bundles backed up at ~/Library/Application Support/Rango Labs/MoteField/Backups/install-Jxk23qFp. Mac ZIP payload verification and CRC passed. Actual listening/DAW automation playback remains user validation; separate browser color controls are not integrated.


## macOS 0.3.11 beta — 2026-09-21

Installed universal AU and VST3 with 43 factory presets, parameter-driven reactor scan/pitch/stretch/split feedback, drag readouts, green dot-matrix bypass indicator, and lime jelly as the default reactor material over the original transparent enclosure. The six new presets are appended; existing program indices and parameter IDs are preserved. Separate reactor/casing color controls remain a browser prototype and are not in this build. Native Settings retains its previous accent controls.

Release build and DSP/click tests passed (84.63 seconds combined). Final native UI/preset regression and reactor host-notification/visual tests passed. Installed bundle versions are 0.3.11, both arm64+x86_64, strict ad-hoc signatures valid, full file manifests identical to dist/macos. Installed AU validation passed, with the existing MIDI-on-aufx warning. DAWs were closed before replacement. Previous 0.3.10 bundles backed up at ~/Library/Application Support/Rango Labs/MoteField/Backups/install-tU5vBWID. User presets and exports preserved.

Mac sharing ZIP: dist/friend-test/MoteField-0.3.11-Mac-Test.zip; manifest/signature/architecture verification and ZIP CRC passed. Windows remains 0.3.10; no new Windows build or public push. Actual Ableton automation recording/playback and listening approval remain unverified. No signing/notarization status changed.
