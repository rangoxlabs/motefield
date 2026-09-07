; Compile through scripts/package-windows.ps1 using Inno Setup 6.3 or newer.
[Setup]
AppId=com.rangolabs.motefield
AppName=MoteField
AppVersion={#AppVersion}
AppPublisher=Rango Labs
DefaultDirName={autopf}\Rango Labs\MoteField
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
OutputDir={#OutputDir}
OutputBaseFilename=MoteField-{#AppVersion}-Windows-x64{#FileSuffix}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=MoteField VST3 — Rango Labs
CloseApplications=yes
RestartApplications=no
#ifdef SignedBuild
SignTool=RangoSign
SignedUninstaller=yes
#endif

[Files]
Source: "{#PluginSource}\*"; DestDir: "{commoncf64}\VST3\MoteField.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
WelcomeLabel2=This installs MoteField by Rango Labs for 64-bit VST3 hosts.%n%nQuit your DAW before continuing. After installation, reopen your DAW and rescan plugins.%n%nRecorded phrase audio saves with your project and user presets.
