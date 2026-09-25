# scripts/common.ps1
#
# Shared helpers for the Okkhor installer/dev/release scripts.
#
# Dot-source this file; it does nothing if run directly:
#
#   . (Join-Path $PSScriptRoot 'common.ps1')
#
# NOTE: install.ps1 (the "irm | iex" online installer) intentionally does
# NOT use this file. When piped through iex it has no local files to dot-
# source, so it stays fully self-contained. Every other script here runs
# from disk (the repo, or the installed {app} folder) and can safely use
# this module.

$OkkhorGitHubOwner = 'okkhor-bn'
$OkkhorGitHubRepo  = 'okkhor-windows'
$OkkhorDllName     = 'okkhor_tsf.dll'
$OkkhorInstallDir  = Join-Path $env:ProgramFiles 'Okkhor'
$OkkhorRegsvr32    = Join-Path $env:WINDIR 'System32\regsvr32.exe'

function Write-Step {
    param([string]$Message)
    Write-Host ''
    Write-Host $Message -ForegroundColor Cyan
}

function Test-IsAdministrator {
    $identity  = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Assert-Administrator {
    if (-not (Test-IsAdministrator)) {
        throw 'Run this script from an elevated PowerShell prompt.'
    }
}

function Get-OkkhorProcesses {
    # Returns the processes that currently have okkhor_tsf.dll loaded, as
    # {Name, Id} objects taken straight from tasklist (no extra Get-Process
    # name lookup needed - tasklist already gives us the PID).
    $output = tasklist /m $OkkhorDllName 2>$null
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

function Stop-OkkhorProcesses {
    # Stops every process that currently has okkhor_tsf.dll loaded.
    $procs = @(Get-OkkhorProcesses)
    foreach ($proc in $procs) {
        Write-Host "  Stopping $($proc.Name) (PID $($proc.Id))..."
        try {
            Stop-Process -Id $proc.Id -Force -ErrorAction Stop
        }
        catch {
            Write-Warning "Could not stop $($proc.Name) (PID $($proc.Id))."
        }
    }
    if ($procs.Count -gt 0) {
        Start-Sleep -Milliseconds 500
    }
}

function Unregister-OkkhorDll {
    param([string]$DllPath)
    if (Test-Path -LiteralPath $DllPath) {
        Write-Host "Unregistering $DllPath..."
        & $OkkhorRegsvr32 /u /s $DllPath
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "regsvr32 /u returned exit code $LASTEXITCODE."
        }
        else {
            Write-Host 'Unregistration completed.'
        }
    }
    else {
        Write-Host "DLL not found, nothing to unregister: $DllPath"
    }
}

function Register-OkkhorDll {
    param([string]$DllPath)

    if (-not (Test-Path -LiteralPath $DllPath -PathType Leaf)) {
        throw "DLL not found: $DllPath"
    }

    Write-Host "Registering $DllPath..."

    & "$env:WINDIR\System32\regsvr32.exe" /s "$DllPath"

    $exitCode = $LASTEXITCODE

    Write-Host "regsvr32 exit code: $exitCode"

    if ($exitCode -eq 0) {
        Write-Host 'Registration successful.' -ForegroundColor Green
    }
    else {
        Write-Warning "regsvr32 returned exit code $exitCode."
    }

    return $exitCode
}