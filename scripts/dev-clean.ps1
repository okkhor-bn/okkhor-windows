# scripts/dev-clean.ps1
#
# Removes a local development install: stops any process holding the TSF
# DLL, unregisters it, and deletes the build directory.
#
#   powershell -ExecutionPolicy Bypass -File scripts\dev-clean.ps1
#
# To unregister a specific DLL (e.g. a Release build) instead of the
# default Debug one:
#
#   powershell -ExecutionPolicy Bypass -File scripts\dev-clean.ps1 -Dll "build\Release\okkhor_tsf.dll"
#
# To unregister without deleting the build directory:
#
#   powershell -ExecutionPolicy Bypass -File scripts\dev-clean.ps1 -KeepBuildDir

param(
    [string]$Dll,
    [switch]$KeepBuildDir
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

Assert-Administrator

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build'

if ($Dll) {
    $dllPath = (Resolve-Path -LiteralPath $Dll -ErrorAction Stop).Path
}
else {
    $dllPath = Join-Path $buildDir 'Debug\okkhor_tsf.dll'
}

Write-Host ''
Write-Host '=== Okkhor dev clean ===' -ForegroundColor Cyan
Write-Host ''

Write-Step 'Stopping Windows components...'
Stop-OkkhorProcesses

Write-Step 'Unregistering Okkhor...'
Unregister-OkkhorDll -DllPath $dllPath

if (-not $KeepBuildDir) {
    if (Test-Path -LiteralPath $buildDir) {
        Write-Step 'Removing build directory...'
        Remove-Item -LiteralPath $buildDir -Recurse -Force
        Write-Host "Removed: $buildDir"
    }
    else {
        Write-Step 'No build directory to remove.'
    }
}
else {
    Write-Step 'Keeping build directory (-KeepBuildDir).'
}

Write-Host ''
Write-Host '=== Okkhor dev clean complete ===' -ForegroundColor Green
Write-Host ''
