# scripts/install.ps1
#
# Installs the Okkhor TSF text service.
#
# Run from an elevated PowerShell:
#
#   powershell -ExecutionPolicy Bypass -File scripts\install.ps1 -Dll build\Debug\okkhor_tsf.dll

param(
    [Parameter(Mandatory = $true)]
    [string]$Dll,

    [string]$DataDir
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Require Administrator
# ---------------------------------------------------------------------------

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)

if (-not $principal.IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator
)) {
    throw 'Run this script from an elevated PowerShell prompt.'
}

# ---------------------------------------------------------------------------
# Resolve DLL path
# ---------------------------------------------------------------------------

$dllPath = (Resolve-Path $Dll).Path

Write-Host "Registering $dllPath"

# ---------------------------------------------------------------------------
# Resolve data directory
# ---------------------------------------------------------------------------

if (-not $DataDir) {
    $candidate = Join-Path (Split-Path $dllPath) 'data'

    if (Test-Path (Join-Path $candidate 'vowels.json')) {
        $DataDir = $candidate
    }
}

if ($DataDir) {
    $DataDir = (Resolve-Path $DataDir).Path

    # Store the data directory as a per-user override.
    New-Item -Path 'HKCU:\Software\Okkhor' -Force | Out-Null

    Set-ItemProperty `
        -Path 'HKCU:\Software\Okkhor' `
        -Name 'DataDir' `
        -Value $DataDir

    Write-Host "Data directory: $DataDir"
}
else {
    Write-Warning 'No data directory found; the service will load but will not transliterate.'
}

# ---------------------------------------------------------------------------
# Register COM / TSF server
# ---------------------------------------------------------------------------

$regsvr32 = Join-Path $env:WINDIR 'System32\regsvr32.exe'

Write-Host 'Registering COM/TSF server...'

& $regsvr32 /s $dllPath

Write-Host 'Registration command completed.'

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------

Write-Host ''
Write-Host 'Registered successfully.'
Write-Host ''
Write-Host 'Add it under:'
Write-Host 'Settings > Time & language > Language & region >'
Write-Host 'Bangla (Bangladesh) > Language options > Keyboards'
Write-Host ''
Write-Host 'Then select "Okkhor Phonetic".'
Write-Host ''
Write-Host 'A 64-bit DLL only serves 64-bit applications.'
Write-Host 'Build and register the 32-bit DLL as well if you want Okkhor'
Write-Host 'to work in 32-bit applications.'
