#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${project_dir}/build-macos"
dist_dir="${project_dir}/dist/macos"

cmake -S "${project_dir}" -B "${build_dir}" -G Xcode \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DMOTEFIELD_BUILD_TESTS=ON

cmake --build "${build_dir}" --config Release --parallel 2
ctest --test-dir "${build_dir}" -C Release --output-on-failure

mkdir -p "${dist_dir}"
ditto "${build_dir}/MoteField_artefacts/Release/VST3/MoteField.vst3" "${dist_dir}/MoteField.vst3"
ditto "${build_dir}/MoteField_artefacts/Release/AU/MoteField.component" "${dist_dir}/MoteField.component"
codesign --force --deep --sign - "${dist_dir}/MoteField.vst3"
codesign --force --deep --sign - "${dist_dir}/MoteField.component"

echo "Built MoteField VST3 and AU in ${dist_dir}"

