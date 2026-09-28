; Framecraft for Windows installer (Inno Setup 6). Per-user install: no administrator rights needed.
; Build: iscc /DAppVersion=1.0.0 /DSourceDir=..\build\Release installer\Framecraft.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\build\Release"
#endif

[Setup]
AppId={{B6E2C1F4-7A3D-4B8E-9C1A-FC0FC0FC0001}
AppName=Framecraft
AppVersion={#AppVersion}
AppVerName=Framecraft {#AppVersion}
AppPublisher=Framecraft
DefaultDirName={localappdata}\Programs\Framecraft
DefaultGroupName=Framecraft
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..\dist
OutputBaseFilename=Framecraft-Setup-{#AppVersion}
SetupIconFile=..\app\Assets\Framecraft.ico
UninstallDisplayIcon={app}\Framecraft.exe
WizardStyle=modern
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
CloseApplications=yes

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.pdb,*.lib,*.exp,*.ilk"

[Icons]
Name: "{autoprograms}\Framecraft"; Filename: "{app}\Framecraft.exe"
Name: "{autodesktop}\Framecraft"; Filename: "{app}\Framecraft.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Framecraft.exe"; Description: "{cm:LaunchProgram,Framecraft}"; Flags: nowait postinstall skipifsilent
