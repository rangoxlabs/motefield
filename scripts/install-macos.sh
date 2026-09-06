#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "This script installs native macOS bundles. Run it on your Mac after build-macos.sh."
  exit 1
fi

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
dist_dir="${project_dir}/dist/macos"
for bundle in MoteField.vst3 MoteField.component; do
  if [[ ! -d "${dist_dir}/${bundle}" ]]; then
    echo "Missing ${bundle}. Run bash scripts/build-macos.sh first."
    exit 1
  fi
done

backup_root="${HOME}/Library/Application Support/Rango Labs/MoteField/Backups"
mkdir -p "${backup_root}"
backup_dir="$(mktemp -d "${backup_root}/install-XXXXXXXX")"

install_bundle() {
  local bundle_name="$1"
  local destination_dir="$2"
  mkdir -p "${destination_dir}"
  local staging_dir
  staging_dir="$(mktemp -d "${destination_dir}/.motefield-stage-XXXXXXXX")"
  ditto "${dist_dir}/${bundle_name}" "${staging_dir}/${bundle_name}"
  codesign --verify --deep --strict "${staging_dir}/${bundle_name}"
  if [[ -e "${destination_dir}/${bundle_name}" ]]; then
    mv "${destination_dir}/${bundle_name}" "${backup_dir}/${bundle_name}"
  fi
  if ! mv "${staging_dir}/${bundle_name}" "${destination_dir}/${bundle_name}"; then
    if [[ -e "${backup_dir}/${bundle_name}" ]]; then
      mv "${backup_dir}/${bundle_name}" "${destination_dir}/${bundle_name}"
    fi
    echo "Installation failed; the previous bundle was restored when available."
    exit 1
  fi
  rmdir "${staging_dir}"
}

install_bundle MoteField.vst3 "${HOME}/Library/Audio/Plug-Ins/VST3"
install_bundle MoteField.component "${HOME}/Library/Audio/Plug-Ins/Components"
echo "Installed MoteField by Rango Labs. Reopen your DAW and rescan plugins if needed."
echo "Any previous bundles are in ${backup_dir}"
