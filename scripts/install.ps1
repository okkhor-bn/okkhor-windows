# scripts/install.ps1
#
# Downloads and launches the latest Okkhor installer.
#
#   irm https://raw.githubusercontent.com/okkhor-bn/okkhor-windows/main/scripts/install.ps1 | iex
#
# This script does the minimum needed to get OkkhorSetup.exe (built by
# release.ps1 from installer.iss) onto the machine and running. Elevation,
# file copy, and TSF registration (register.ps1) are all handled by the
# installer itself - this script does not duplicate that logic.
#
# For local/dev installs from a freshly built DLL, use dev-build.ps1
# instead; it registers a DLL directly without going through Setup.

$ErrorActionPreference = 'Stop'

$GitHubOwner = 'okkhor-bn'
$GitHubRepo  = 'okkhor-windows'
$AssetName   = 'OkkhorSetup.exe'

Write-Host ''
Write-Host '========================================' -ForegroundColor Cyan
Write-Host '          Okkhor Installer' -ForegroundColor Cyan
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ''

if (-not [Environment]::Is64BitOperatingSystem) {
    throw 'Okkhor currently requires 64-bit Windows.'
}

Write-Host 'Checking latest stable release...'
$releaseApi = "https://api.github.com/repos/$GitHubOwner/$GitHubRepo/releases/latest"
$release = Invoke-RestMethod -Uri $releaseApi -Headers @{ 'User-Agent' = 'Okkhor-Installer' }

if (-not $release.assets) {
    throw 'The latest Okkhor release does not contain any downloadable assets.'
}

$asset = $release.assets | Where-Object { $_.name -eq $AssetName } | Select-Object -First 1
if (-not $asset) {
    throw "The latest Okkhor release does not contain $AssetName."
}

Write-Host "Release: $($release.tag_name)"
Write-Host "Asset:   $($asset.name)"

$tempRoot  = Join-Path $env:TEMP ("okkhor-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
$setupPath = Join-Path $tempRoot $asset.name

try {
    Write-Host ''
    Write-Host 'Downloading installer...'
    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $setupPath

    Write-Host 'Launching installer (you may see a UAC prompt)...'
    Write-Host ''
    $process = Start-Process -FilePath $setupPath -Wait -PassThru
    exit $process.ExitCode
}
finally {
    Start-Sleep -Seconds 1
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}
