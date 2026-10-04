#define AppName "DirZero"
#define AppVersion "1.0.0"
#define AppPublisher "DirZero contributors"
#define StageDir "..\..\build\windows-package"

[Setup]
AppId={{D47E8711-5A01-4A0D-AB1A-6D83CBA2BD4A}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\DirZero
DefaultGroupName=DirZero
DisableProgramGroupPage=yes
LicenseFile=..\..\LICENSE
OutputDir=..\..\dist
OutputBaseFilename=DirZero-Setup-{#AppVersion}
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayIcon={app}\DirZero.exe
Compression=lzma2
SolidCompression=yes
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\DirZero"; Filename: "{app}\bin\DirZero.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\DirZero"; Filename: "{app}\bin\DirZero.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\DirZero.exe"; Description: "Launch DirZero"; Flags: postinstall nowait skipifsilent
