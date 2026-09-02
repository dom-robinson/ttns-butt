; TTNS Deck + TTNS Remote — Inno Setup installer (double-click .exe).
; Built from dist/windows/ttns-deck-win64/ after scripts/build-windows.sh
#ifndef MyAppVersion
  #define MyAppVersion "0.1.16-ttns-remote.2"
#endif

#define MyAppName "TTNS Deck"
#define MyAppPublisher "The Thursday Night Show"
#define MyAppURL "https://thethursdaynightshow.com"
#define SrcRoot "..\.."

[Setup]
AppId={{8F3A1B2C-4D5E-4670-8192-A1B2C3D4E5F6}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={localappdata}\Programs\TTNS Deck
DefaultGroupName=TTNS Deck
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir={#SrcRoot}\dist\windows
OutputBaseFilename=TTNS-Deck-{#MyAppVersion}-windows-x64-setup
SetupIconFile={#SrcRoot}\assets\ttns-deck.ico
UninstallDisplayIcon={app}\bin\ttns-deck.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
RestartApplications=no
LicenseFile={#SrcRoot}\COPYING

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"

[Files]
Source: "{#SrcRoot}\dist\windows\ttns-deck-win64\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\TTNS Deck"; Filename: "{app}\bin\ttns-deck.exe"; WorkingDir: "{app}"; Comment: "TTNS live mixer"
Name: "{group}\TTNS Remote"; Filename: "{app}\bin\ttns-remote.exe"; WorkingDir: "{app}"; Comment: "TTNS co-host dial-in"; Check: FileExists(ExpandConstant('{app}\bin\ttns-remote.exe'))
Name: "{autodesktop}\TTNS Deck"; Filename: "{app}\bin\ttns-deck.exe"; WorkingDir: "{app}"; Tasks: desktopicon
Name: "{autodesktop}\TTNS Remote"; Filename: "{app}\bin\ttns-remote.exe"; WorkingDir: "{app}"; Tasks: desktopicon; Check: FileExists(ExpandConstant('{app}\bin\ttns-remote.exe'))

[Run]
Filename: "{app}\bin\ttns-deck.exe"; Description: "Launch TTNS Deck"; Flags: nowait postinstall skipifsilent; WorkingDir: "{app}"
