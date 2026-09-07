param([string]$BuildDir = '')
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
if (-not $BuildDir) { $BuildDir = Join-Path $projectDir 'build-windows' }
$source = Join-Path $BuildDir 'MoteField_artefacts/Release/VST3/MoteField.vst3'
$version = (Get-Content "$source/Contents/Resources/moduleinfo.json" -Raw | ConvertFrom-Json).Version
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'Invalid version' }
$output = Join-Path $projectDir 'dist/friend-test'
$work = Join-Path $output ('.windows-kit-' + [guid]::NewGuid())
$name = "MoteField-$version-Windows-Test"
$kit = Join-Path $work $name
New-Item -ItemType Directory -Force "$kit/Plug-ins" | Out-Null
try {
    Copy-Item -LiteralPath $source -Destination "$kit/Plug-ins/MoteField.vst3" -Recurse
    Copy-Item "$projectDir/packaging/friend-windows/*" $kit
    $payload = Join-Path $kit 'Plug-ins/MoteField.vst3'
    $manifest = @(Get-ChildItem $payload -Recurse -File | ForEach-Object { @{path=[IO.Path]::GetRelativePath($payload,$_.FullName);sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash} })
    ConvertTo-Json -InputObject $manifest | Set-Content -Encoding utf8 "$kit/SHA256SUMS.json"
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$kit/Install.ps1" -VerifyOnly
    if ($LASTEXITCODE -ne 0) { throw 'Kit validation failed' }
    $zip = Join-Path $output "$name.zip"
    Compress-Archive -LiteralPath $kit -DestinationPath $zip -Force
    $test = Join-Path $work 'extracted'; Expand-Archive -LiteralPath $zip -DestinationPath $test
    $script = Join-Path $test "$name/Install.ps1"
    $root = Join-Path $work 'TestInstall'
    for ($attempt=0; $attempt -lt 2; $attempt++) {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script -TestRoot $root
        if ($LASTEXITCODE -ne 0) { throw 'ZIP installation test failed' }
    }
    $binary = "$root/VST3/MoteField.vst3/Contents/x86_64-win/MoteField.vst3"
    if ((Get-FileHash $binary).Hash -ne (Get-FileHash "$source/Contents/x86_64-win/MoteField.vst3").Hash) { throw 'Installed binary differs from build' }
    if (@(Get-ChildItem "$root/Backups" -Directory).Count -ne 1) { throw 'Reinstallation backup failed' }
    Add-Content "$test/$name/Plug-ins/MoteField.vst3/Contents/x86_64-win/MoteField.vst3" 'corruption-test'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script -TestRoot "$work/Rejected"
    if ($LASTEXITCODE -eq 0 -or (Test-Path "$work/Rejected")) { throw 'Modified payload was not rejected' }
    $hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $name.zip" | Set-Content -Encoding ascii "$zip.sha256"
    Write-Host "Created and verified $zip (fresh install, backup, binary hash, corrupt-payload rejection)"
} finally { Remove-Item -LiteralPath $work -Recurse -Force }
