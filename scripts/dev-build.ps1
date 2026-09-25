# scripts/dev-build.ps1
#
# Okkhor development build cycle:
#
#   1. Detect processes that may hold the TSF DLL and offer to stop them
#   2. Build the Debug configuration (skipped if -Dll is supplied)
#   3. Unregister the previous DLL and register the new/supplied one
#
# Run from the repository root, as Administrator:
#
#   powershell -ExecutionPolicy Bypass -File scripts\dev-build.ps1
#
# To skip the build and just (re)register an already-built DLL:
#
#   powershell -ExecutionPolicy Bypass -File scripts\dev-build.ps1 -Dll "build\Debug\okkhor_tsf.dll"

param(
    [string]$Dll
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
Write-Host '=== Okkhor development build ===' -ForegroundColor Cyan
Write-Host ''

# ------------------------------------------------------------
# Offer to stop processes that may hold the TSF DLL
# ------------------------------------------------------------
$processes = @(Get-OkkhorProcesses)
if ($processes.Count -gt 0) {
    Write-Host '[1/3] Processes that may be using Okkhor:'
    Write-Host ''
    for ($i = 0; $i -lt $processes.Count; $i++) {
        Write-Host "  [$($i + 1)] $($processes[$i].Name) (PID $($processes[$i].Id))"
    }
    Write-Host ''
    Write-Host 'These processes can keep the TSF DLL loaded.'
    Write-Host ''
    Write-Host 'Options:'
    Write-Host '  A = Kill all'
    Write-Host '  N = Kill none'
    Write-Host '  1..N = Kill selected process'
    Write-Host ''
    $choice = Read-Host 'Choose'

    if ($choice -match '^[Aa]$') {
        Stop-OkkhorProcesses
    }
    elseif ($choice -match '^[Nn]$') {
        Write-Host '      No processes stopped.'
    }
    elseif ($choice -match '^[0-9]+$') {
        $index = [int]$choice - 1
        if ($index -lt 0 -or $index -ge $processes.Count) {
            throw 'Invalid process selection.'
        }
        $process = $processes[$index]
        Write-Host "      Stopping $($process.Name) (PID $($process.Id))..."
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }
    else {
        throw 'Invalid choice.'
    }

    Start-Sleep -Milliseconds 500
}
else {
    Write-Host '[1/3] No known TSF processes are currently running.'
}

# ------------------------------------------------------------
# Build (skipped when a DLL was supplied directly)
# ------------------------------------------------------------
if (-not $Dll) {
    Write-Host ''
    Write-Host '[2/3] Building Debug configuration...'
    Write-Host ''
    Push-Location $repoRoot
    try {
        cmake --build $buildDir --config Debug
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }

    if (-not (Test-Path -LiteralPath $dllPath)) {
        throw "Build completed but DLL was not found: $dllPath"
    }
}
else {
    Write-Host ''
    Write-Host '[2/3] Using supplied DLL, skipping build...'
    Write-Host "      DLL: $dllPath"
}

# ------------------------------------------------------------
# Register
# ------------------------------------------------------------
Write-Host ''
Write-Host '[3/3] Registering new TSF DLL...'
Write-Host ''
Unregister-OkkhorDll -DllPath $dllPath
Register-OkkhorDll -DllPath $dllPath | Out-Null

Write-Host ''
Write-Host '=== Okkhor build completed successfully ===' -ForegroundColor Green
Write-Host ''
Write-Host "DLL: $dllPath"
Write-Host ''
Write-Host 'Switch to Okkhor Phonetic and test it.'
Write-Host ''
