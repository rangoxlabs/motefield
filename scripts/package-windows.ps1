param(
    [string]$BuildDir = "",
    [switch]$Unsigned,
    [string]$CertificateThumbprint = $env:WINDOWS_CERTIFICATE_THUMBPRINT,
    [string]$ArtifactSigningDlib = "",
    [string]$ArtifactSigningMetadata = "",
    [switch]$AAX,
    [string]$SignedAaxPath = "",
    [string]$PaceWrapTool = "wraptool.exe",
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
    & python "$projectDir/scripts/collect-notices.py" --juce-dir "$BuildDir/_deps/juce-src" --output "$stage/Notices"
    if ($LASTEXITCODE -ne 0) { throw 'Collecting third-party notices failed' }
    $suffix = "-UNSIGNED"
    $signOptions = @()
    $aaxOptions = @()
    if ($AAX) {
        $aaxSource = if ($SignedAaxPath) { $SignedAaxPath } else { Join-Path $BuildDir 'MoteField_artefacts/Release/AAX/MoteField.aaxplugin' }
        $aaxBinary = Join-Path $aaxSource 'Contents/x64/MoteField.aaxplugin'
        if (-not (Test-Path $aaxBinary)) { throw 'Missing Windows AAX Native bundle' }
        $aaxVersion = (Get-Item $aaxBinary).VersionInfo.ProductVersion
        if ($aaxVersion -ne $version) { throw "AAX version $aaxVersion differs from VST3 version $version" }
        Copy-Item -Recurse $aaxSource "$stage/MoteField.aaxplugin"
        $aaxOptions = @("/DAaxSource=$stage/MoteField.aaxplugin")
        if (-not $Unsigned) {
            if (-not $SignedAaxPath) { throw 'Release AAX requires -SignedAaxPath from the PACE signing pipeline' }
            & $PaceWrapTool verify --in "$stage/MoteField.aaxplugin"
            if ($LASTEXITCODE -ne 0) { throw 'PACE signature verification failed' }
        }
    }
    if (-not $Unsigned) {
        $signTool = (Get-Command signtool.exe -ErrorAction Stop).Source
        if ($ArtifactSigningDlib -or $ArtifactSigningMetadata) {
            if ($CertificateThumbprint) { throw 'Select either certificate-store or Artifact Signing, not both' }
            $dlib = (Resolve-Path -LiteralPath $ArtifactSigningDlib -ErrorAction Stop).Path
            $metadataPath = (Resolve-Path -LiteralPath $ArtifactSigningMetadata -ErrorAction Stop).Path
            $signingMetadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
            if (-not $signingMetadata.Endpoint -or -not $signingMetadata.CodeSigningAccountName -or -not $signingMetadata.CertificateProfileName) { throw 'Incomplete Artifact Signing metadata' }
            if (-not $PSBoundParameters.ContainsKey('TimestampUrl')) { $TimestampUrl = 'http://timestamp.acs.microsoft.com' }
            $providerArgs = @('/dlib', $dlib, '/dmdf', $metadataPath)
            $providerCommand = "/dlib `$q$dlib`$q /dmdf `$q$metadataPath`$q"
        } else {
            if ($CertificateThumbprint -notmatch '^[0-9A-Fa-f]{40}$') { throw 'Configure a certificate thumbprint or Artifact Signing, or explicitly use -Unsigned' }
            $providerArgs = @('/sha1', $CertificateThumbprint)
            $providerCommand = "/sha1 $CertificateThumbprint"
        }
        & $signTool sign @providerArgs /fd SHA256 /tr $TimestampUrl /td SHA256 "$stage/MoteField.vst3/Contents/x86_64-win/MoteField.vst3"
        if ($LASTEXITCODE -ne 0) { throw "Plugin signing failed" }
        & $signTool verify /pa /all "$stage/MoteField.vst3/Contents/x86_64-win/MoteField.vst3"
        if ($LASTEXITCODE -ne 0) { throw "Plugin signature verification failed" }
        $suffix = ""
        if ($AAX) {
            & $signTool verify /pa /all "$stage/MoteField.aaxplugin/Contents/x64/MoteField.aaxplugin"
            if ($LASTEXITCODE -ne 0) { throw 'AAX Authenticode verification failed' }
        }
        $signCommand = "`$q$signTool`$q sign $providerCommand /fd SHA256 /tr `$q$TimestampUrl`$q /td SHA256 `$f"
        $signOptions = @('/DSignedBuild=1', "/SRangoSign=$signCommand")
    }
    if ($AAX) { $suffix = "-AAX$suffix" }
    & $ISCC "/DPluginSource=$stage/MoteField.vst3" "/DNoticesSource=$stage/Notices" "/DAppVersion=$version" "/DOutputDir=$outputDir" "/DFileSuffix=$suffix" @signOptions @aaxOptions "$projectDir/packaging/windows/MoteField.iss"
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
