#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
source_dir="$PWD/Plug-ins"
install_root="$HOME/Library"
verify_only=false
if [[ "${1:-}" == "--verify-only" ]]; then verify_only=true; fi
# Used by the packaging checks to exercise installation without touching a real DAW.
if [[ "${1:-}" == "--test-root" && -n "${2:-}" ]]; then install_root="$2"; fi
if [[ "$(uname -s)" != "Darwin" ]]; then echo "This download is for macOS, not Windows."; exit 1; fi
printf '\nMoteField by Rango Labs - private Mac test build\n\n'
shasum -a 256 -c SHA256SUMS.txt
for name in MoteField.vst3 MoteField.component; do
    codesign --verify --deep --strict "$source_dir/$name"
done
if $verify_only; then echo "Payload checks passed. Nothing was installed."; exit 0; fi
printf '\nClose your DAW before installing. No administrator password is needed.\n'
if [[ "${1:-}" != "--test-root" ]]; then
    read -r -p 'DAW closed and ready to install this trusted Rango Labs test build? [y/N] ' reply
    case "$reply" in y|Y|yes|YES) ;; *) echo "Nothing installed."; exit 0 ;; esac
fi
backup_root="$install_root/Application Support/Rango Labs/MoteField/Backups"
mkdir -p "$backup_root"
backup_dir="$(mktemp -d "$backup_root/friend-test-XXXXXXXX")"
for name in MoteField.vst3 MoteField.component; do
    if [[ "$name" == *.vst3 ]]; then format=VST3; else format=Components; fi
    destination="$install_root/Audio/Plug-Ins/$format"
    mkdir -p "$destination"
    staging="$(mktemp -d "$destination/.motefield-install-XXXXXXXX")"
    ditto "$source_dir/$name" "$staging/$name"
    # This exception is limited to the two plug-ins copied from this verified test kit.
    # It does not change Gatekeeper or any system-wide security setting.
    xattr -dr com.apple.quarantine "$staging/$name"
    codesign --verify --deep --strict "$staging/$name"
    if [[ -e "$destination/$name" ]]; then mv "$destination/$name" "$backup_dir/$name"; fi
    if ! mv "$staging/$name" "$destination/$name"; then
        if [[ -e "$backup_dir/$name" ]]; then mv "$backup_dir/$name" "$destination/$name"; fi
        echo "Installation failed; the previous copy was restored when available."; exit 1
    fi
    rmdir "$staging"
done
printf '\nInstalled successfully. Reopen your DAW and rescan plug-ins.\n'
printf 'Previous copies, if any: %s\n' "$backup_dir"
printf 'Look for MoteField under Rango Labs in the VST3 or Audio Units list.\n'
