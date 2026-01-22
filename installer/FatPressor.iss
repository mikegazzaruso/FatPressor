; FatPressor Windows Installer Script
; Requires Inno Setup 6.x

#define MyAppName "FatPressor"
#define MyAppVersion "0.2.0"
#define MyAppPublisher "Sylfo"
#define MyAppURL "https://sylfo.space"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppPublisher}
OutputDir=..\build\installer
OutputBaseFilename=FatPressor-{#MyAppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; VST3 Plugin
Source: "..\build\FatPressor_artefacts\Release\VST3\FatPressor.vst3\*"; DestDir: "{commoncf64}\VST3\FatPressor.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

; Standalone (optional)
Source: "..\build\FatPressor_artefacts\Release\Standalone\FatPressor.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\FatPressor.exe"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\FatPressor.exe"; Description: "Launch FatPressor"; Flags: nowait postinstall skipifsilent

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
  MsgBox('FatPressor will install:' + #13#10 + #13#10 +
         '- VST3 plugin to: C:\Program Files\Common Files\VST3\' + #13#10 +
         '- Standalone app to: ' + ExpandConstant('{autopf}') + '\Sylfo\FatPressor\',
         mbInformation, MB_OK);
end;
