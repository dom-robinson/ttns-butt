; TTNS Remote — Inno Setup installer (double-click .exe) for co-hosts.
; Built from dist/windows/ttns-deck-win64/ after scripts/build-windows.sh
#ifndef MyAppVersion
  #define MyAppVersion "0.1.16-ttns-remote.3"
#endif

#define MyAppName "TTNS Remote"
#define MyAppPublisher "The Thursday Night Show"
#define MyAppURL "https://thethursdaynightshow.com"
#define SrcRoot "..\.."

[Setup]
AppId={{9A4B2C3D-5E6F-4781-92A3-B4C5D6E7F809}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={localappdata}\Programs\TTNS Remote
DefaultGroupName=TTNS Remote
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir={#SrcRoot}\dist\windows
OutputBaseFilename=TTNS-Remote-{#MyAppVersion}-windows-x64-setup
SetupIconFile={#SrcRoot}\assets\ttns-deck.ico
UninstallDisplayIcon={app}\bin\ttns-remote.exe
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
Source: "{#SrcRoot}\dist\windows\ttns-deck-win64\bin\ttns-remote.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#SrcRoot}\dist\windows\ttns-deck-win64\bin\*.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#SrcRoot}\dist\windows\ttns-deck-win64\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SrcRoot}\dist\windows\ttns-deck-win64\legal\*"; DestDir: "{app}\legal"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SrcRoot}\dist\windows\ttns-deck-win64\Run TTNS Remote.bat"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SrcRoot}\dist\windows\ttns-deck-win64\USER_GUIDE.md"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SrcRoot}\dist\windows\ttns-deck-win64\README-WINDOWS.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\TTNS Remote"; Filename: "{app}\bin\ttns-remote.exe"; WorkingDir: "{app}"; Comment: "TTNS co-host dial-in"
Name: "{autodesktop}\TTNS Remote"; Filename: "{app}\bin\ttns-remote.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\ttns-remote.exe"; Description: "Launch TTNS Remote"; Flags: nowait postinstall skipifsilent; WorkingDir: "{app}"
