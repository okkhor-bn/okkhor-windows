# scripts/dev-build.ps1
#
# Okkhor development build cycle:
#
#   1. Detect processes that may hold the TSF DLL
#   2. Ask which processes to terminate
#   3. Unregister the current TSF DLL
#   4. Build the Debug configuration
#   5. Register the newly built DLL
#
# Run from the repository root:
#
#   powershell -ExecutionPolicy Bypass -File scripts\dev-build.ps1
#
# Run PowerShell as Administrator.

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build'
$dllPath = Join-Path $buildDir 'Debug\okkhor_tsf.dll'
$installScript = Join-Path $repoRoot 'scripts\install.ps1'
$regsvr32 = Join-Path $env:WINDIR 'System32\regsvr32.exe'

Write-Host ''
Write-Host '=== Okkhor development build ==='
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
# Find processes that may hold the TSF DLL
# ------------------------------------------------------------

function Get-OkkhorProcesses {

    $processNames = @(
        'explorer',
        'TextInputHost',
        'ctfmon'
    )

    $result = @()

    foreach ($name in $processNames) {

        $processes = Get-Process `
            -Name $name `
            -ErrorAction SilentlyContinue

        foreach ($process in $processes) {

            $result += $process
        }
    }

    return $result
}

$processes = @(Get-OkkhorProcesses)

if ($processes.Count -gt 0) {

    Write-Host '[0/3] Processes that may be using Okkhor:'
    Write-Host ''

    for ($i = 0; $i -lt $processes.Count; $i++) {

        $process = $processes[$i]

        Write-Host "  [$($i + 1)] $($process.ProcessName) (PID $($process.Id))"
    }

    Write-Host ''
    Write-Host 'These processes can keep the TSF DLL loaded.'
    Write-Host ''
    Write-Host 'Options:'
    Write-Host '  A = Kill all'
    Write-Host '  N = Kill none'
    Write-Host '  1..N = Kill selected processes'
    Write-Host ''

    $choice = Read-Host 'Choose'

    if ($choice -match '^[Aa]$') {

        foreach ($process in $processes) {

            Write-Host `
                "      Stopping $($process.ProcessName) (PID $($process.Id))..."

            Stop-Process `
                -Id $process.Id `
                -Force `
                -ErrorAction SilentlyContinue
        }
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

        Write-Host `
            "      Stopping $($process.ProcessName) (PID $($process.Id))..."

        Stop-Process `
            -Id $process.Id `
            -Force `
            -ErrorAction SilentlyContinue
    }
    else {

        throw 'Invalid choice.'
    }

    Start-Sleep -Milliseconds 500
}
else {

    Write-Host '[0/3] No known TSF processes are currently running.'
}

# ------------------------------------------------------------
# Unregister existing DLL
# ------------------------------------------------------------

if (Test-Path $dllPath) {

    Write-Host ''
    Write-Host '[1/3] Unregistering existing TSF DLL...'

    & $regsvr32 /u /s $dllPath

    if ($LASTEXITCODE -ne 0) {

        Write-Warning `
            "regsvr32 unregistration returned exit code $LASTEXITCODE."
    }
    else {

        Write-Host '      Unregistration completed.'
    }
}
else {

    Write-Host ''
    Write-Host '[1/3] No existing TSF DLL found. Skipping unregister.'
}

# ------------------------------------------------------------
# Build
# ------------------------------------------------------------

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

if (-not (Test-Path $dllPath)) {

    throw "Build completed but DLL was not found: $dllPath"
}

# ------------------------------------------------------------
# Register new DLL
# ------------------------------------------------------------

Write-Host ''
Write-Host '[3/3] Registering new TSF DLL...'
Write-Host ''

& $installScript -Dll $dllPath

if ($LASTEXITCODE -ne 0) {

    throw "Installation failed with exit code $LASTEXITCODE."
}

Write-Host ''
Write-Host '=== Okkhor build completed successfully ==='
Write-Host ''
Write-Host "DLL: $dllPath"
Write-Host ''
Write-Host 'Switch to Okkhor Phonetic and test it.'
Write-Host ''

Write-Host 'Installation complete.' -ForegroundColor Green
Write-Host ''
Write-Host 'Press any key to exit...'
$null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')