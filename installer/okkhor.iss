; installer/installer.iss
;
; Inno Setup installer for Okkhor.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#define AppName "Okkhor"
#define AppPublisher "okkhor-bn"
#define AppURL "https://github.com/okkhor-bn/okkhor-windows"
#define DllName "okkhor_tsf.dll"

[Setup]
AppId={{6B2B9C2E-6E3B-4B7B-9C9A-6C6E9E6B3E4A}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}

DefaultDirName={autopf}\Okkhor

DisableDirPage=yes
DisableProgramGroupPage=yes
DisableWelcomePage=no

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

PrivilegesRequired=admin

OutputBaseFilename=OkkhorSetup

Compression=lzma2
SolidCompression=yes
WizardStyle=modern

UninstallDisplayIcon={app}\{#DllName}

SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\build\Release\{#DllName}"; \
    DestDir: "{app}"; \
    Flags: ignoreversion

Source: "..\scripts\register.ps1"; \
    DestDir: "{app}\scripts"; \
    Flags: ignoreversion

Source: "..\scripts\uninstall.ps1"; \
    DestDir: "{app}\scripts"; \
    Flags: ignoreversion

Source: "..\scripts\common.ps1"; \
    DestDir: "{app}\scripts"; \
    Flags: ignoreversion

[Run]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
    Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\scripts\register.ps1"" -Dll ""{app}\{#DllName}"""; \
    StatusMsg: "Registering Okkhor..."; \
    Flags: runhidden waituntilterminated

[UninstallRun]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
    Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\scripts\uninstall.ps1"" -Dll ""{app}\{#DllName}"""; \
    StatusMsg: "Unregistering Okkhor..."; \
    Flags: runhidden waituntilterminated