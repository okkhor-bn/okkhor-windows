# scripts/uninstall.ps1
#
# Removes the Okkhor text service. Elevated PowerShell required.
#
#     powershell -ExecutionPolicy Bypass -File scripts\uninstall.ps1 -Dll build\Debug\okkhor_tsf.dll
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Dll,
    [switch]$KeepDataDirSetting
)

$ErrorActionPreference = 'Stop'

$identity  = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script from an elevated PowerShell prompt.'
}

$dllPath = (Resolve-Path $Dll).Path
& regsvr32.exe /u /s $dllPath
if ($LASTEXITCODE -ne 0) { throw "regsvr32 /u failed with exit code $LASTEXITCODE" }

if (-not $KeepDataDirSetting) {
    Remove-Item -Path 'HKCU:\Software\Okkhor' -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'Unregistered. Sign out and back in if Windows still lists the keyboard.'
