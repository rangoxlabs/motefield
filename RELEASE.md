# Release preparation

## Current status

Version **0.3.5** targets universal macOS VST3/AU and Windows x64 VST3. See [VERIFICATION.md](VERIFICATION.md) for build, installation and host-test results.

The repository includes macOS and Windows build jobs that produce unsigned development installers. Public release signing, notarization, clean-machine installation, upgrades, and native host testing remain release checks. Generated bundles and installers are excluded from source control.

## macOS

```sh
bash scripts/build-macos.sh
bash scripts/package-macos.sh --unsigned
```

Development installers are written to `dist/installers`. The package installs VST3 and AU bundles into `/Library/Audio/Plug-Ins` for all users. Quit the DAW first. A copy under the user's Library can shadow the system installation; use one installation location when testing upgrades.

For release packaging, configure the publisher's Developer ID Application and Developer ID Installer identities. Store notarization credentials using the interactive prompts in `xcrun notarytool store-credentials`, then run:

```sh
export DEVELOPER_ID_APPLICATION='Developer ID Application: YOUR LEGAL ENTITY (TEAMID)'
export DEVELOPER_ID_INSTALLER='Developer ID Installer: YOUR LEGAL ENTITY (TEAMID)'
export NOTARY_PROFILE='rango-labs-notary'
bash scripts/package-macos.sh --release
```

Release mode signs staged plug-ins with hardened runtime and timestamp, signs the installer, submits it for notarization, checks acceptance, staples the ticket, and verifies the result. Keep passwords, certificates, and private keys outside the repository.

## Windows

Use Visual Studio with Desktop development with C++, CMake 3.22+, Git, PowerShell 7, and Inno Setup 6.3 or newer:

```powershell
./scripts/build-windows.ps1
./scripts/package-windows.ps1 -Unsigned
```

The installer places the full bundle at `C:\Program Files\Common Files\VST3\MoteField.vst3` and adds an uninstaller. The build targets x64 with the static MSVC runtime.

For signed packaging, configure a code-signing identity in the current-user certificate store, make Windows SDK `signtool.exe` available on PATH, and run:

```powershell
./scripts/package-windows.ps1 -CertificateThumbprint 'YOUR_40_CHARACTER_CERTIFICATE_THUMBPRINT'
```

The script signs and verifies the plug-in and configures Inno Setup to sign the installer and uninstaller. Other signing providers may require integration changes.

## Automation and distribution

[Build plug-ins](.github/workflows/build.yml) runs on pushes to `main` or manual dispatch. It builds macOS universal and Windows x64 targets, runs DSP tests, packages unsigned installers, and uploads workflow artifacts. It does not create a public GitHub release or use release-signing credentials.

Before distributing release binaries:

- Confirm the applicable JUCE license and include the required third-party notices.
- Test clean installation, upgrades, uninstall behavior, and host scanning.
- Test audio playback, automation recording/playback, preset saving, and session recall in the intended DAWs.
- Exercise Apple Silicon, Intel macOS, and Windows x64 hosts separately.
- Record phrase-loop output before closing a session; phrase audio is currently temporary.

An iOS edition requires separate app/AUv3 targets and distribution work; it is not included in the desktop build.
