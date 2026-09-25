# scripts/uninstall.ps1
#
# Unregisters the Okkhor TSF DLL. Intended to run as an Inno Setup
# pre-uninstall [UninstallRun] step, before Setup removes the files:
#
#   [UninstallRun]
#   Filename: "powershell.exe"; \
#     Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\scripts\uninstall.ps1"" -Dll ""{app}\okkhor_tsf.dll"""; \
#     Flags: runhidden waituntilterminated
#
# This script only stops processes and unregisters the DLL. It does NOT
# delete files or registry keys - Inno's own uninstaller already does
# that ([Files]/[UninstallDelete] entries, and any [Registry] entries
# tagged uninsdeletekey), so duplicating it here would just be two things
# fighting over the same cleanup. If you need to preserve a user setting
# across uninstall (the old script's -KeepDataDirSetting), do that with
# an uninsdeletekey exclusion in installer.iss instead.

param(
    [string]$Dll
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

Assert-Administrator

if ([string]::IsNullOrWhiteSpace($Dll)) {
    $dllPath = Join-Path $OkkhorInstallDir $OkkhorDllName
}
else {
    $dllPath = (Resolve-Path -LiteralPath $Dll -ErrorAction Stop).Path
}

Write-Step 'Stopping Windows components...'
Stop-OkkhorProcesses

Write-Step 'Unregistering Okkhor...'
Unregister-OkkhorDll -DllPath $dllPath

Write-Host ''
Write-Host '=== Okkhor unregistered ===' -ForegroundColor Green
Write-Host ''
