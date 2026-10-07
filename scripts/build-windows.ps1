param([string]$BuildDir = "", [int]$Jobs = 2, [switch]$AAX, [string]$AaxSdkPath = "")
$ErrorActionPreference = "Stop"
$projectDir = Split-Path $PSScriptRoot -Parent
if (-not $BuildDir) { $BuildDir = Join-Path $projectDir "build-windows" }
$aaxFlag = if ($AAX) { 'ON' } else { 'OFF' }
& cmake -S $projectDir -B $BuildDir -A x64 -DMOTEFIELD_BUILD_TESTS=ON "-DMOTEFIELD_BUILD_AAX=$aaxFlag" "-DMOTEFIELD_AAX_SDK_PATH=$AaxSdkPath" '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>'
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
& cmake --build $BuildDir --config Release --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw "Windows build failed" }
& ctest --test-dir $BuildDir -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "DSP tests failed" }
Write-Host "Built Windows x64 VST3. Package with scripts/package-windows.ps1 -BuildDir `"$BuildDir`" -Unsigned"
