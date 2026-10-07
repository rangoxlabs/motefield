param([switch]$AAX)
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$version = (Get-Content "$projectDir/build-windows/MoteField_artefacts/Release/VST3/MoteField.vst3/Contents/Resources/moduleinfo.json" -Raw | ConvertFrom-Json).Version
$suffix = if ($AAX) { '-AAX-UNSIGNED' } else { '-UNSIGNED' }
$installer = Get-Item "$projectDir/dist/installers/MoteField-$version-Windows-x64$suffix.exe"
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
$aaxInstalled = Join-Path $env:CommonProgramFiles 'Avid/Audio/Plug-Ins/MoteField.aaxplugin'
if ($AAX) {
    $aaxSource = "$projectDir/build-windows/MoteField_artefacts/Release/AAX/MoteField.aaxplugin"
    $aaxFiles = @(Get-ChildItem -LiteralPath $aaxSource -Recurse -File)
    foreach ($file in $aaxFiles) {
        $relative = [IO.Path]::GetRelativePath($aaxSource, $file.FullName)
        if ((Get-FileHash (Join-Path $aaxInstalled $relative)).Hash -ne (Get-FileHash $file.FullName).Hash) { throw "AAX payload differs: $relative" }
    }
    if (@(Get-ChildItem -LiteralPath $aaxInstalled -Recurse -File).Count -ne $aaxFiles.Count) { throw 'Unexpected AAX payload files' }
    Write-Host 'Verified complete installed AAX payload'
}
$uninstaller = Join-Path $env:ProgramFiles 'Rango Labs/MoteField/unins000.exe'
if (-not (Test-Path $uninstaller)) { throw 'Uninstaller missing' }
$result = Start-Process -FilePath $uninstaller -ArgumentList '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART' -Wait -PassThru
if ($result.ExitCode -ne 0) { throw "Uninstaller failed: $($result.ExitCode)" }
if (Test-Path $binary) { throw 'Uninstaller left the plugin binary behind' }
if ($AAX -and (Test-Path "$aaxInstalled/Contents/x64/MoteField.aaxplugin")) { throw 'Uninstaller left the AAX binary behind' }
Write-Host 'Windows installer and uninstaller checks passed'
