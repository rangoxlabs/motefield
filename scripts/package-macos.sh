#!/usr/bin/env bash
set -euo pipefail

# Release mode deliberately requires both signing identities and notarization.
mode="${1:---release}"
include_aax="${2:-}"
if [[ ( "$mode" != --release && "$mode" != --unsigned ) || ( -n "$include_aax" && "$include_aax" != --include-aax ) ]]; then
  echo "Usage: bash scripts/package-macos.sh [--release|--unsigned] [--include-aax]" >&2
  exit 2
fi
[[ "$(uname -s)" == Darwin ]] || { echo "Requires macOS." >&2; exit 1; }
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
source_dir="${project_dir}/dist/macos"
output_dir="${project_dir}/dist/installers"
version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "${source_dir}/MoteField.vst3/Contents/Info.plist")"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Invalid bundle version." >&2; exit 1; }
if [[ "$mode" == --release ]]; then
  : "${DEVELOPER_ID_APPLICATION:?Set your Developer ID Application identity}"
  : "${DEVELOPER_ID_INSTALLER:?Set your Developer ID Installer identity}"
  : "${NOTARY_PROFILE:?Set the name of your notarytool keychain profile}"
fi
mkdir -p "$output_dir"
work_dir="$(mktemp -d "${output_dir}/.macos-package-XXXXXXXX")"
trap 'rm -rf "$work_dir"' EXIT
payload="${work_dir}/payload"
for format in vst3 component; do
  bundle="MoteField.${format}"
  destination=VST3
  [[ "$format" != component ]] || destination=Components
  original="${source_dir}/${bundle}"
  [[ -d "$original" ]] || { echo "Missing ${original}; run build-macos.sh first." >&2; exit 1; }
  bundle_version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "${original}/Contents/Info.plist")"
  [[ "$bundle_version" == "$version" ]] || { echo "Bundle versions differ." >&2; exit 1; }
  lipo "${original}/Contents/MacOS/MoteField" -verify_arch arm64 x86_64
  target="${payload}/Library/Audio/Plug-Ins/${destination}/${bundle}"
  mkdir -p "$(dirname "$target")"
  ditto "$original" "$target"
  if [[ "$mode" == --release ]]; then
    codesign --force --options runtime --timestamp --sign "$DEVELOPER_ID_APPLICATION" "$target"
  else
    codesign --force --sign - "$target"
  fi
  codesign --verify --deep --strict "$target"
done

if [[ "$include_aax" == --include-aax ]]; then
  original="${MOTEFIELD_SIGNED_AAX_PATH:-${source_dir}/MoteField.aaxplugin}"
  [[ -d "$original" ]] || { echo "Missing AAX bundle: $original" >&2; exit 1; }
  bundle_version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "${original}/Contents/Info.plist")"
  [[ "$bundle_version" == "$version" ]] || { echo "AAX version differs." >&2; exit 1; }
  lipo "${original}/Contents/MacOS/MoteField" -verify_arch arm64 x86_64
  target="${payload}/Library/Application Support/Avid/Audio/Plug-Ins/MoteField.aaxplugin"
  mkdir -p "$(dirname "$target")"
  ditto "$original" "$target"
  if [[ "$mode" == --release ]]; then
    # Eden covers the whole bundle. Never re-sign/modify it with codesign here.
    : "${MOTEFIELD_SIGNED_AAX_PATH:?Set the path of the final PACE-signed AAX bundle}"
    "${PACE_WRAPTOOL:-wraptool}" verify --in "$target"
    codesign --verify --deep --strict "$target"
    codesign -dvv "$target" 2>&1 | grep -q 'Authority=Developer ID Application:' || {
      echo "AAX must also carry a Developer ID Application signature." >&2; exit 1;
    }
  fi
fi

# Disable bundle relocation so a previous per-user copy cannot redirect installation.
python3 "${project_dir}/scripts/collect-notices.py" \
  --juce-dir "${project_dir}/build-macos/_deps/juce-src" \
  --output "${payload}/Library/Application Support/Rango Labs/MoteField/Notices"
pkgbuild --analyze --root "$payload" "${work_dir}/components.plist"
python3 - "${work_dir}/components.plist" <<'PY'
import plistlib, sys
with open(sys.argv[1], 'rb') as f:
    components = plistlib.load(f)
for component in components:
    component['BundleIsRelocatable'] = False
    component['BundleOverwriteAction'] = 'upgrade'
with open(sys.argv[1], 'wb') as f:
    plistlib.dump(components, f)
PY
pkgbuild --root "$payload" --component-plist "${work_dir}/components.plist" \
  --identifier com.rangolabs.motefield.pkg --version "$version" \
  --install-location / --ownership recommended "${work_dir}/payload.pkg"
cat > "${work_dir}/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>MoteField — Rango Labs</title>
  <welcome file="welcome.html" mime-type="text/html"/>
  <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_localSystem="true" enable_currentUserHome="false" enable_anywhere="false"/>
  <volume-check script="true"><allowed-os-versions><os-version min="11.0"/></allowed-os-versions></volume-check>
  <choices-outline><line choice="default"/></choices-outline>
  <choice id="default" visible="false"><pkg-ref id="com.rangolabs.motefield.pkg"/></choice>
  <pkg-ref id="com.rangolabs.motefield.pkg" version="${version}" onConclusion="none">payload.pkg</pkg-ref>
</installer-gui-script>
EOF
suffix=""
sign_args=()
if [[ "$mode" == --release ]]; then
  sign_args=(--sign "$DEVELOPER_ID_INSTALLER" --timestamp)
else
  suffix="-UNSIGNED"
fi
package="${output_dir}/MoteField-${version}-macOS-universal${suffix}.pkg"
[[ -z "$include_aax" ]] || package="${output_dir}/MoteField-${version}-macOS-universal-AAX${suffix}.pkg"
productbuild --distribution "${work_dir}/distribution.xml" --package-path "$work_dir" \
  --resources "${project_dir}/packaging/macos" ${sign_args[@]+"${sign_args[@]}"} "${work_dir}/installer.pkg"
if [[ "$mode" == --release ]]; then
  xcrun notarytool submit "${work_dir}/installer.pkg" --keychain-profile "$NOTARY_PROFILE" \
    --wait --output-format json > "${output_dir}/MoteField-${version}-notary.json"
  python3 - "${output_dir}/MoteField-${version}-notary.json" <<'PY'
import json, sys
with open(sys.argv[1]) as f:
    result = json.load(f)
if result.get('status') != 'Accepted':
    raise SystemExit('Notarization failed; inspect the notary JSON and fetch the submission log.')
PY
  xcrun stapler staple "${work_dir}/installer.pkg"
  xcrun stapler validate "${work_dir}/installer.pkg"
  pkgutil --check-signature "${work_dir}/installer.pkg"
  spctl --assess --type install --verbose "${work_dir}/installer.pkg"
fi
mv "${work_dir}/installer.pkg" "$package"
shasum -a 256 "$package" > "${package}.sha256"
echo "Created ${package}"
[[ "$mode" != --unsigned ]] || echo "Development installer only: unsigned and not notarized."
