$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$installer = Get-ChildItem "$projectDir/dist/installers/MoteField-*-Windows-x64-UNSIGNED.exe" | Select-Object -First 1
if (-not $installer) { throw 'Missing installer' }
$installed = Join-Path $env:CommonProgramFiles 'VST3/MoteField.vst3'
$binary = Join-Path $installed 'Contents/x86_64-win/MoteField.vst3'
$source = Join-Path $projectDir 'build-windows/MoteField_artefacts/Release/VST3/MoteField.vst3/Contents/x86_64-win/MoteField.vst3'
$result = Start-Process -FilePath $installer.FullName -ArgumentList '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-' -Wait -PassThru
if ($result.ExitCode -ne 0) { throw "Installer failed: $($result.ExitCode)" }
if (-not (Test-Path $binary)) { throw 'Plugin not installed in the standard VST3 folder' }
if ((Get-FileHash $binary).Hash -ne (Get-FileHash $source).Hash) { throw 'Installed binary differs from build' }
$metadata = Get-Content "$installed/Contents/Resources/moduleinfo.json" -Raw | ConvertFrom-Json
Write-Host "Verified installed MoteField $($metadata.Version), matching the built binary"
$uninstaller = Join-Path $env:ProgramFiles 'Rango Labs/MoteField/unins000.exe'
if (-not (Test-Path $uninstaller)) { throw 'Uninstaller missing' }
$result = Start-Process -FilePath $uninstaller -ArgumentList '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART' -Wait -PassThru
if ($result.ExitCode -ne 0) { throw "Uninstaller failed: $($result.ExitCode)" }
if (Test-Path $binary) { throw 'Uninstaller left the plugin binary behind' }
Write-Host 'Windows installer and uninstaller checks passed'
