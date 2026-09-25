; installer/installer.iss
;
; Inno Setup script for Okkhor. Repository layout assumed:
;
;   /installer/installer.iss   (this file)
;   /scripts/register.ps1
;   /scripts/uninstall.ps1
;   /scripts/common.ps1
;   /builds/Release/okkhor_tsf.dll
;
; Built by scripts/release.ps1:
;
;   iscc installer\installer.iss /Odist /DAppVersion=1.2.0
;
; AppVersion can be passed with /DAppVersion=x.y.z (release.ps1 does this
; via -Version); it falls back to 0.0.0 for a manual/local compile so the
; script still works without that switch.

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
Source: "..\build\Release\{#DllName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\scripts\register.ps1";      DestDir: "{app}\scripts"; Flags: ignoreversion
Source: "..\scripts\uninstall.ps1";     DestDir: "{app}\scripts"; Flags: ignoreversion
Source: "..\scripts\common.ps1";        DestDir: "{app}\scripts"; Flags: ignoreversion

[Run]
; register.ps1 stops any process holding the DLL, unregisters whatever is
; already registered at this path (harmless on a first install), then
; registers the DLL that was just copied above.
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
  Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\scripts\register.ps1"" -Dll ""{app}\{#DllName}"""; \
  StatusMsg: "Registering Okkhor..."; \
  Flags: runhidden waituntilterminated

[UninstallRun]
; Inno runs [UninstallRun] entries before it removes files, so the DLL is
; still on disk when uninstall.ps1 unregisters it. File/registry removal
; is left to Inno itself (see [Files]/uninsdeletekey), not duplicated here.
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
  Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\scripts\uninstall.ps1"" -Dll ""{app}\{#DllName}"""; \
  StatusMsg: "Unregistering Okkhor..."; \
  Flags: runhidden waituntilterminated

[Code]
// On an upgrade/repair, the old DLL may still be loaded by a running
// process (explorer, a text editor, etc.), which would make the [Files]
// copy step above fail with a sharing violation before register.ps1 ever
// gets a chance to stop anything. Stop-Okkhor here, right before Inno
// starts copying files, so upgrades over a live install don't need the
// user to manually close apps and hit Retry.
procedure StopRunningOkkhorProcesses();
var
  ResultCode: Integer;
  Command: String;
begin
  Command := '-NoProfile -Command "$ids = @(tasklist /m {#DllName} 2>$null | ' +
    'ForEach-Object { if ($_ -match ''^(?<n>\S+)\s+(?<id>\d+)\s'') { [int]$matches[''id''] } }); ' +
    'foreach ($procId in $ids) { Stop-Process -Id $procId -Force -ErrorAction SilentlyContinue } "';
  Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'), Command, '',
    SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
    StopRunningOkkhorProcesses();
end;
