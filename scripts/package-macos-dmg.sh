#!/usr/bin/env bash
# Direct-download AU/VST3 disk image, signed and notarized without a PKG identity.
set -euo pipefail
: "${DEVELOPER_ID_APPLICATION:?Set your Developer ID Application identity}"
: "${NOTARY_PROFILE:?Set a validated notarytool keychain profile}"
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
source_dir="${project_dir}/dist/macos"
version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "${source_dir}/MoteField.vst3/Contents/Info.plist")"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'Invalid version' >&2; exit 1; }
output_dir="${project_dir}/dist/installers"
mkdir -p "$output_dir"
work_dir="$(mktemp -d "${output_dir}/.dmg-package-XXXXXXXX")"
trap 'rm -rf "$work_dir"' EXIT
payload="${work_dir}/MoteField"
mkdir -p "$payload/AU" "$payload/VST3"
for format in vst3 component; do
  destination=VST3
  [[ "$format" != component ]] || destination=AU
  bundle="MoteField.$format"
  test "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "${source_dir}/${bundle}/Contents/Info.plist")" = "$version"
  lipo "${source_dir}/${bundle}/Contents/MacOS/MoteField" -verify_arch arm64 x86_64
  ditto "${source_dir}/${bundle}" "${payload}/${destination}/${bundle}"
  codesign --force --options runtime --timestamp --sign "$DEVELOPER_ID_APPLICATION" "${payload}/${destination}/${bundle}"
  codesign --verify --deep --strict "${payload}/${destination}/${bundle}"
done
python3 "${project_dir}/scripts/collect-notices.py" --juce-dir "${project_dir}/build-macos/_deps/juce-src" --output "${payload}/Notices"
cat > "${payload}/INSTALL.txt" <<'EOF'
MoteField — Rango Labs
AU and VST3 for macOS 11 or later, Apple Silicon and Intel.

1. Quit your DAW.
2. In Finder, choose Go > Go to Folder and enter:
   ~/Library/Audio/Plug-Ins/
3. Copy AU/MoteField.component into Components, and
   VST3/MoteField.vst3 into VST3. Create those folders if needed.
4. If replacing a previous MoteField, first move that bundle to a backup folder.
   Keep just one installed copy of each format; a copy in your user Library
   can shadow an older one in /Library/Audio/Plug-Ins/.
5. Reopen your DAW and rescan plug-ins. Look for MoteField by Rango Labs.

Your presets and recorded loop/session data are separate from the plug-in bundles.
To uninstall, quit the DAW and remove only the two installed MoteField bundles.

Pro Tools/AAX is distributed separately. This download contains AU/VST3 only.
https://www.rangolabs.co
EOF
name="MoteField-${version}-macOS-universal"
hdiutil create -volname "MoteField ${version}" -srcfolder "$payload" -format UDZO -ov "${work_dir}/${name}.dmg"
codesign --force --timestamp --sign "$DEVELOPER_ID_APPLICATION" "${work_dir}/${name}.dmg"
codesign --verify --strict "${work_dir}/${name}.dmg"
xcrun notarytool submit "${work_dir}/${name}.dmg" --keychain-profile "$NOTARY_PROFILE" \
  --wait --output-format json > "${output_dir}/${name}-dmg-notary.json"
python3 - "${output_dir}/${name}-dmg-notary.json" <<'PY'
import json, sys
with open(sys.argv[1]) as f:
    result = json.load(f)
if result.get('status') != 'Accepted':
    raise SystemExit('Notarization was not accepted; inspect the submission log.')
PY
xcrun stapler staple "${work_dir}/${name}.dmg"
xcrun stapler validate "${work_dir}/${name}.dmg"
spctl --assess --type open --context context:primary-signature --verbose "${work_dir}/${name}.dmg"
mv "${work_dir}/${name}.dmg" "${output_dir}/${name}.dmg"
shasum -a 256 "${output_dir}/${name}.dmg" > "${output_dir}/${name}.dmg.sha256"
echo "Created signed, notarized and stapled ${output_dir}/${name}.dmg"
