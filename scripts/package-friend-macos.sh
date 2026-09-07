#!/bin/bash
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
source_dir="${1:-$project_dir/dist/macos}"
version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$source_dir/MoteField.vst3/Contents/Info.plist")"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Invalid version"; exit 1; }
output_dir="$project_dir/dist/friend-test"
mkdir -p "$output_dir"
work="$(mktemp -d "$output_dir/.kit-XXXXXXXX")"
trap 'rm -rf "$work"' EXIT
name="MoteField-$version-Mac-Test"
kit="$work/$name"
mkdir -p "$kit/Plug-ins"
for bundle in MoteField.vst3 MoteField.component; do
    bundle_version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$source_dir/$bundle/Contents/Info.plist")"
    [[ "$version" == "$bundle_version" ]] || { echo "Bundle versions differ"; exit 1; }
    lipo "$source_dir/$bundle/Contents/MacOS/MoteField" -verify_arch arm64 x86_64
    ditto "$source_dir/$bundle" "$kit/Plug-ins/$bundle"
    codesign --verify --deep --strict "$kit/Plug-ins/$bundle"
done
cp "$project_dir/packaging/friend-macos/Install.command" "$kit/Install.command"
sed "s/@VERSION@/$version/g" "$project_dir/packaging/friend-macos/START HERE.txt" > "$kit/START HERE.txt"
chmod +x "$kit/Install.command"
(cd "$kit" && find Plug-ins -type f -exec shasum -a 256 '{}' \; > SHA256SUMS.txt)
bash "$kit/Install.command" --verify-only
ditto -c -k --sequesterRsrc --keepParent "$kit" "$output_dir/$name.zip"
shasum -a 256 "$output_dir/$name.zip" > "$output_dir/$name.zip.sha256"
echo "Created $output_dir/$name.zip"
