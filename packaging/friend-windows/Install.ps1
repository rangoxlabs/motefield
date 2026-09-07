param([string]$TestRoot = '', [switch]$VerifyOnly)
$ErrorActionPreference = 'Stop'
try {
    $source = Join-Path $PSScriptRoot 'Plug-ins/MoteField.vst3'
    $manifest = Get-Content (Join-Path $PSScriptRoot 'SHA256SUMS.json') -Raw | ConvertFrom-Json
    $files = @(Get-ChildItem -LiteralPath $source -Recurse -File)
    if ($files.Count -ne @($manifest).Count) { throw 'Payload file count differs from the checksum manifest' }
    foreach ($entry in $manifest) {
        $path = [IO.Path]::GetFullPath((Join-Path $source $entry.path))
        if (-not $path.StartsWith($source + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid checksum path' }
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.sha256) { throw "Checksum failed: $($entry.path)" }
    }
    if (-not (Test-Path (Join-Path $source 'Contents/x86_64-win/MoteField.vst3'))) { throw 'Windows VST3 binary missing' }
    Write-Host 'MoteField payload checks passed.'
    if ($VerifyOnly) { exit 0 }
    if (-not $TestRoot) {
        $admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
        if (-not $admin) {
            $process = Start-Process powershell.exe -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`"" -Wait -PassThru
            exit $process.ExitCode
        }
        Write-Host 'Save your session and quit your DAW before continuing.'
        $reply = Read-Host 'Install this trusted Rango Labs test build? [y/N]'
        if ($reply -notmatch '^(y|yes)$') { Write-Host 'Nothing installed.'; exit 0 }
        $destinationRoot = Join-Path $env:CommonProgramW6432 'VST3'
        $backupRoot = Join-Path $env:ProgramData 'Rango Labs/MoteField/Backups'
    } else {
        $destinationRoot = Join-Path $TestRoot 'VST3'
        $backupRoot = Join-Path $TestRoot 'Backups'
    }
    New-Item -ItemType Directory -Force $destinationRoot,$backupRoot | Out-Null
    $destination = Join-Path $destinationRoot 'MoteField.vst3'
    $stage = Join-Path $destinationRoot ('.motefield-stage-' + [guid]::NewGuid())
    $backup = Join-Path $backupRoot ('MoteField-' + [guid]::NewGuid() + '.vst3')
    Copy-Item -LiteralPath $source -Destination $stage -Recurse
    $hadPrevious = Test-Path -LiteralPath $destination
    try {
        if ($hadPrevious) { Move-Item -LiteralPath $destination -Destination $backup }
        Move-Item -LiteralPath $stage -Destination $destination
    } catch {
        if ($hadPrevious -and (Test-Path -LiteralPath $backup) -and -not (Test-Path -LiteralPath $destination)) { Move-Item -LiteralPath $backup -Destination $destination }
        throw
    } finally { if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force } }
    Write-Host "Installed successfully: $destination"
    if ($hadPrevious) { Write-Host "Previous version backed up: $backup" }
    Write-Host 'Reopen your DAW, rescan VST3 plugins, and find MoteField by Rango Labs.'
    if (-not $TestRoot) { Read-Host 'Press Enter to close' | Out-Null }
} catch {
    Write-Host "Installation failed: $_" -ForegroundColor Red
    if (-not $TestRoot -and -not $VerifyOnly) { Read-Host 'Press Enter to close' | Out-Null }
    exit 1
}
