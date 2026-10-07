#!/usr/bin/env bash
# Prepare Developer ID-signed AU/VST3 payloads when installer/notary setup is pending.
# This is deliberately not a distributable release or an alternative to notarization.
set -euo pipefail
: "${DEVELOPER_ID_APPLICATION:?Set your Developer ID Application identity}"
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
source_dir="${project_dir}/dist/macos"
version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "${source_dir}/MoteField.vst3/Contents/Info.plist")"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'Invalid version' >&2; exit 1; }
output="${project_dir}/dist/signing/MoteField-${version}-macOS-SIGNED-NOT-NOTARIZED"
[[ ! -e "$output" ]] || { echo "Already exists: $output" >&2; exit 1; }
mkdir -p "$output"
for format in vst3 component; do
  bundle="MoteField.$format"
  test "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "${source_dir}/${bundle}/Contents/Info.plist")" = "$version"
  lipo "${source_dir}/${bundle}/Contents/MacOS/MoteField" -verify_arch arm64 x86_64
  ditto "${source_dir}/${bundle}" "${output}/${bundle}"
  codesign --force --options runtime --timestamp --sign "$DEVELOPER_ID_APPLICATION" "${output}/${bundle}"
  codesign --verify --deep --strict "${output}/${bundle}"
  codesign -dvv "${output}/${bundle}" 2> "${output}/${bundle}.signature.txt"
done
python3 "${project_dir}/scripts/collect-notices.py" --juce-dir "${project_dir}/build-macos/_deps/juce-src" --output "${output}/Notices"
echo 'Internal signing checkpoint. NOT NOTARIZED. Do not distribute as a release.' > "${output}/STATUS.txt"
ditto -c -k --keepParent "$output" "${output}.zip"
shasum -a 256 "${output}.zip" > "${output}.zip.sha256"
echo "Prepared ${output}.zip; Apple notarization and signed installer remain pending."
