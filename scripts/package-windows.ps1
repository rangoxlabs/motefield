param(
    [string]$BuildDir = "",
    [switch]$Unsigned,
    [string]$CertificateThumbprint = $env:WINDOWS_CERTIFICATE_THUMBPRINT,
    [string]$TimestampUrl = "http://timestamp.digicert.com",
    [string]$ISCC = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
)
$ErrorActionPreference = "Stop"
$projectDir = Split-Path $PSScriptRoot -Parent
if (-not $BuildDir) { $BuildDir = Join-Path $projectDir "build-windows" }
$bundle = Join-Path $BuildDir "MoteField_artefacts/Release/VST3/MoteField.vst3"
if (-not (Test-Path "$bundle/Contents/x86_64-win/MoteField.vst3")) { throw "Missing Windows x64 bundle; run build-windows.ps1 first" }
$metadata = Get-Content "$bundle/Contents/Resources/moduleinfo.json" -Raw | ConvertFrom-Json
$version = $metadata.Version
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "Invalid version in moduleinfo.json" }
if (-not (Test-Path $ISCC)) { throw "Install Inno Setup 6 or pass -ISCC with the compiler path" }
$outputDir = Join-Path $projectDir "dist/installers"
$stage = Join-Path $projectDir "dist/windows-stage/$([guid]::NewGuid())"
New-Item -ItemType Directory -Force $stage, $outputDir | Out-Null
try {
    Copy-Item -Recurse $bundle "$stage/MoteField.vst3"
    $suffix = "-UNSIGNED"
    $signOptions = @()
    if (-not $Unsigned) {
        if ($CertificateThumbprint -notmatch '^[0-9A-Fa-f]{40}$') { throw "Pass a Windows code-signing certificate SHA-1 thumbprint, or explicitly use -Unsigned for testing" }
        $signTool = (Get-Command signtool.exe -ErrorAction Stop).Source
        & $signTool sign /sha1 $CertificateThumbprint /fd SHA256 /tr $TimestampUrl /td SHA256 "$stage/MoteField.vst3/Contents/x86_64-win/MoteField.vst3"
        if ($LASTEXITCODE -ne 0) { throw "Plugin signing failed" }
        & $signTool verify /pa /all "$stage/MoteField.vst3/Contents/x86_64-win/MoteField.vst3"
        if ($LASTEXITCODE -ne 0) { throw "Plugin signature verification failed" }
        $suffix = ""
        $signCommand = "`$q$signTool`$q sign /sha1 $CertificateThumbprint /fd SHA256 /tr $TimestampUrl /td SHA256 `$f"
        $signOptions = @('/DSignedBuild=1', "/SRangoSign=$signCommand")
    }
    & $ISCC "/DPluginSource=$stage/MoteField.vst3" "/DAppVersion=$version" "/DOutputDir=$outputDir" "/DFileSuffix=$suffix" @signOptions "$projectDir/packaging/windows/MoteField.iss"
    if ($LASTEXITCODE -ne 0) { throw "Installer compilation failed" }
    $installer = Join-Path $outputDir "MoteField-$version-Windows-x64$suffix.exe"
    if (-not $Unsigned) {
        & $signTool verify /pa /all $installer
        if ($LASTEXITCODE -ne 0) { throw "Installer signature verification failed" }
    }
    $hash = (Get-FileHash -Algorithm SHA256 $installer).Hash.ToLowerInvariant()
    "$hash  $(Split-Path $installer -Leaf)" | Set-Content -Encoding ascii "$installer.sha256"
    Write-Host "Created $installer"
} finally {
    Remove-Item -Recurse -Force $stage
}
