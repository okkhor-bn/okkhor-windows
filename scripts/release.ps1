# scripts/release.ps1
#
# Builds a Release configuration and packages the Okkhor Windows release
# artifacts, ready to attach to a GitHub release:
#
#   - OkkhorSetup.exe          (Inno Setup installer; downloaded/run by install.ps1)
#   - okkhor-windows-x64.zip   (portable DLL, for manual/advanced installs)
#
# Run from the repository root:
#
#   powershell -ExecutionPolicy Bypass -File scripts\release.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\release.ps1 -Version 1.2.0
#
# Requires:
#   - CMake (Release build of okkhor_tsf.dll)
#   - Inno Setup 6 (iscc.exe on PATH, or at its default install location)
#   - installer.iss at the repository root, which must:
#       * install okkhor_tsf.dll and the scripts\ folder (register.ps1,
#         uninstall.ps1, common.ps1) into {app}
#       * call scripts\register.ps1 -Dll "{app}\okkhor_tsf.dll" from [Run]
#       * call scripts\uninstall.ps1 -Dll "{app}\okkhor_tsf.dll" from [UninstallRun]
#   This script does not generate installer.iss - it must already exist.

param(
    [string]$Version
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build'
$dllPath  = Join-Path $buildDir 'Release\okkhor_tsf.dll'
$issPath  = Join-Path $repoRoot '\installer\okkhor.iss'
$distDir  = Join-Path $repoRoot 'dist'
$zipPath  = Join-Path $distDir 'okkhor-windows-x64.zip'

Write-Host ''
Write-Host '=== Okkhor release ===' -ForegroundColor Cyan
Write-Host ''

# ------------------------------------------------------------
# Build
# ------------------------------------------------------------
Write-Step 'Building Release configuration...'
Push-Location $repoRoot
try {
    cmake --build $buildDir --config Release
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

# ------------------------------------------------------------
# Reset dist directory
# ------------------------------------------------------------
if (Test-Path -LiteralPath $distDir) {
    Remove-Item -LiteralPath $distDir -Recurse -Force
}
New-Item -ItemType Directory -Path $distDir -Force | Out-Null

# ------------------------------------------------------------
# Portable zip
# ------------------------------------------------------------
Write-Step 'Packaging portable zip...'
Compress-Archive -Path $dllPath -DestinationPath $zipPath -Force
Write-Host "Created: $zipPath"

# ------------------------------------------------------------
# Inno Setup installer
# ------------------------------------------------------------
Write-Step 'Building Inno Setup installer...'

if (-not (Test-Path -LiteralPath $issPath)) {
    throw "Inno Setup script not found: $issPath"
}

$iscc = Get-Command 'iscc.exe' -ErrorAction SilentlyContinue
if (-not $iscc) {
    $defaultIscc = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'
    if (Test-Path -LiteralPath $defaultIscc) {
        $iscc = Get-Item -LiteralPath $defaultIscc
    }
}
if (-not $iscc) {
    throw 'ISCC.exe (Inno Setup 6 compiler) was not found on PATH or in the default install location.'
}

$isccArgs = @($issPath, "/O$distDir")
if ($Version) {
    $isccArgs += "/DAppVersion=$Version"
}

& $iscc.Source $isccArgs
if ($LASTEXITCODE -ne 0) {
    throw "ISCC failed with exit code $LASTEXITCODE."
}

# ------------------------------------------------------------
# Done
# ------------------------------------------------------------
Write-Host ''
Write-Host '=== Release artifacts ready ===' -ForegroundColor Green
Write-Host ''
Get-ChildItem -Path $distDir | ForEach-Object { Write-Host "  $($_.Name)" }
Write-Host ''
Write-Host "Upload the files in $distDir to the GitHub release."
Write-Host ''
