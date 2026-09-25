# scripts/install.ps1
#
# Downloads and launches the latest Okkhor installer.
#
#   irm https://raw.githubusercontent.com/okkhor-bn/okkhor-windows/main/scripts/install.ps1 | iex
#
# This script does not use the GitHub API.
# GitHub's releases/latest/download URL automatically redirects
# to the asset from the latest published release.
#
# The installer itself handles:
#   - elevation
#   - installing the DLL
#   - TSF registration
#   - shortcuts / application files
#   - uninstallation registration
#
# For local/dev installs from a freshly built DLL, use dev-build.ps1.

$ErrorActionPreference = 'Stop'

$DownloadUrl = 'https://github.com/okkhor-bn/okkhor-windows/releases/latest/download/OkkhorSetup-x64.exe'

Write-Host ''
Write-Host '========================================' -ForegroundColor Cyan
Write-Host '           Okkhor Installer' -ForegroundColor Cyan
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ''

if (-not [Environment]::Is64BitOperatingSystem) {
    throw 'Okkhor currently requires 64-bit Windows.'
}

$tempRoot = Join-Path $env:TEMP (
    'okkhor-' + [Guid]::NewGuid().ToString('N')
)

$setupPath = Join-Path $tempRoot 'OkkhorSetup-x64.exe'

try {
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    Write-Host 'Downloading latest stable Okkhor release...'
    Write-Host ''

    Invoke-WebRequest `
        -Uri $DownloadUrl `
        -OutFile $setupPath

    if (-not (Test-Path -LiteralPath $setupPath)) {
        throw 'The Okkhor installer could not be downloaded.'
    }

    Write-Host 'Download complete.'
    Write-Host ''
    Write-Host 'Launching installer (you may see a UAC prompt)...'
    Write-Host ''

    $process = Start-Process `
        -FilePath $setupPath `
        -Wait `
        -PassThru

    exit $process.ExitCode
}
finally {
    Start-Sleep -Seconds 1

    Remove-Item `
        -LiteralPath $tempRoot `
        -Recurse `
        -Force `
        -ErrorAction SilentlyContinue
}
