# scripts/dev-build.ps1
#
# Development build cycle:
#   1. Unregister the current TSF DLL
#   2. Build the Debug configuration
#   3. Register the newly built DLL
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

# Check administrator privileges

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)

if (-not $principal.IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator
)) {
    throw 'Run this script from an elevated PowerShell prompt.'
}

# Stop common processes that may have loaded the TSF DLL.

Write-Host '[0/3] Stopping processes that may hold the TSF DLL...'

$processNames = @(
    'explorer',
    'TextInputHost',
    'ctfmon'
)

foreach ($name in $processNames) {
    $processes = Get-Process -Name $name -ErrorAction SilentlyContinue

    foreach ($process in $processes) {
        Write-Host "      Stopping $name (PID $($process.Id))..."
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }
}

Start-Sleep -Milliseconds 500

# Unregister existing DLL

if (Test-Path $dllPath) {
    Write-Host '[1/3] Unregistering existing TSF DLL...'

    & $regsvr32 /u /s $dllPath

    Write-Host '      Unregistration command completed.'
}
else {
    Write-Host '[1/3] No existing TSF DLL found. Skipping unregister.'
}

# Build

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

# Register new DLL

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