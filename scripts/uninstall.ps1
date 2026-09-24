# scripts/uninstall.ps1
#
# Removes the Okkhor text service.
#
# Run from an elevated PowerShell:
#
#   powershell -ExecutionPolicy Bypass -File scripts\uninstall.ps1
#
# Optional DLL:
#
#   powershell -ExecutionPolicy Bypass -File scripts\uninstall.ps1 -Dll "path\to\okkhor_tsf.dll"

[CmdletBinding()]
param(
    [switch]$KeepDataDirSetting,

    [string]$Dll
)

$ErrorActionPreference = 'Stop'

$InstallDir = Join-Path $env:ProgramFiles 'Okkhor'

if ([string]::IsNullOrWhiteSpace($Dll)) {
    $DllPath = Join-Path $InstallDir 'okkhor_tsf.dll'
}
else {
    $DllPath = (Resolve-Path -LiteralPath $Dll -ErrorAction Stop).Path
}

$regsvr32 = Join-Path $env:WINDIR 'System32\regsvr32.exe'

Write-Host ''
Write-Host '=== Okkhor uninstall ==='
Write-Host ''

# ------------------------------------------------------------
# Administrator check
# ------------------------------------------------------------

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()

$principal = New-Object Security.Principal.WindowsPrincipal($identity)

if (-not $principal.IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator
    )) {

    throw 'Run this script from an elevated PowerShell prompt.'
}

# ------------------------------------------------------------
# Stop processes that may have loaded the TSF DLL
# ------------------------------------------------------------

function Get-OkkhorProcesses {
    $dllName = 'okkhor_tsf.dll'

    $output = tasklist /m $dllName 2>$null

    $result = @()

    foreach ($line in $output) {

        if ($line -match '^\s*(\S+)\s+(\d+)\s+') {

            $result += [PSCustomObject]@{
                Name = $matches[1]
                Id   = [int]$matches[2]
            }
        }
    }

    return $result
}

$processNames = @(Get-OkkhorProcesses)

foreach ($name in $processNames) {

    $processes = Get-Process `
        -Name $name `
        -ErrorAction SilentlyContinue

    foreach ($process in $processes) {

        Write-Host `
            "Stopping $($process.ProcessName) (PID $($process.Id))..."

        Stop-Process `
            -Id $process.Id `
            -Force `
            -ErrorAction SilentlyContinue
    }
}

Start-Sleep -Milliseconds 500

# ------------------------------------------------------------
# Unregister TSF
# ------------------------------------------------------------

if (Test-Path -LiteralPath $DllPath) {

    Write-Host ''
    Write-Host "Unregistering $DllPath..."

    & $regsvr32 /u /s $DllPath

    if ($LASTEXITCODE -ne 0) {

        throw `
            "regsvr32 /u failed with exit code $LASTEXITCODE."
    }

    Write-Host 'Unregistration completed.'
}
else {

    Write-Host ''
    Write-Host "Okkhor DLL was not found: $DllPath"
    Write-Host 'Skipping TSF unregistration.'
}

# ------------------------------------------------------------
# Remove installed files
# ------------------------------------------------------------

# Only remove the installation directory when using
# the default installed DLL.

if ([string]::IsNullOrWhiteSpace($Dll)) {

    if (Test-Path -LiteralPath $InstallDir) {

        Write-Host ''
        Write-Host "Removing $InstallDir..."

        Remove-Item `
            -Path $InstallDir `
            -Recurse `
            -Force

        Write-Host 'Installation directory removed.'
    }
}
else {

    Write-Host ''
    Write-Host 'Custom DLL supplied.'
    Write-Host 'Skipping removal of the installation directory.'
}

# ------------------------------------------------------------
# Remove registry settings
# ------------------------------------------------------------

if (-not $KeepDataDirSetting) {

    Remove-Item `
        -Path 'HKCU:\Software\Okkhor' `
        -Recurse `
        -Force `
        -ErrorAction SilentlyContinue

    Write-Host 'Okkhor registry settings removed.'
}

# ------------------------------------------------------------
# Restart Windows components
# ------------------------------------------------------------

Write-Host ''
Write-Host 'Restarting Windows components...'

# Start-Process explorer.exe
# Start-Process ctfmon.exe

Write-Host ''
Write-Host '=== Okkhor uninstalled successfully ==='
Write-Host ''

Write-Host 'If Windows still lists Okkhor Phonetic,'
Write-Host 'sign out and sign back in.'
Write-Host ''

Write-Host 'Uninstallation complete.' -ForegroundColor Green
Write-Host ''

Write-Host 'Press any key to exit...'

$null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')

