# scripts/install.ps1
#
# Installs the Okkhor TSF text service.
#
# End-user installation:
#
#   irm https://raw.githubusercontent.com/okkhor-bn/okkhor-windows/main/scripts/install.ps1 | iex
#
# Developer/local installation:
#
#   .\scripts\install.ps1 -Dll build\Debug\okkhor_tsf.dll
#
# When -Dll is omitted, the script downloads the latest stable
# Okkhor Windows release automatically.

param(
    [string]$Dll
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

$GitHubOwner = 'okkhor-bn'
$GitHubRepo = 'okkhor-windows'

$InstallDir = Join-Path $env:ProgramFiles 'Okkhor'

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

function Write-Step {
    param(
        [string]$Message
    )

    Write-Host ''
    Write-Host $Message -ForegroundColor Cyan
}

function Require-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()

    $principal = New-Object Security.Principal.WindowsPrincipal($identity)

    if (-not $principal.IsInRole(
            [Security.Principal.WindowsBuiltInRole]::Administrator
        )) {
        return $false
    }

    return $true
}

function Start-ElevatedInstaller {
    param(
        [string]$ScriptPath,
        [string]$DllPath
    )

    Write-Host ''
    Write-Host 'Administrator privileges are required.' -ForegroundColor Yellow
    Write-Host 'Requesting permission through Windows UAC...' -ForegroundColor Yellow
    Write-Host ''

    $arguments = @(
        '-NoProfile'
        '-ExecutionPolicy'
        'Bypass'
        '-File'
        "`"$ScriptPath`""
    )

    if ($DllPath) {
        $arguments += @(
            '-Dll'
            "`"$DllPath`""
        )
    }

    $process = Start-Process `
        -FilePath 'powershell.exe' `
        -ArgumentList $arguments `
        -Verb RunAs `
        -Wait `
        -PassThru

    exit $process.ExitCode
}

# ---------------------------------------------------------------------------
# Banner
# ---------------------------------------------------------------------------

Write-Host ''
Write-Host '========================================' -ForegroundColor Cyan
Write-Host '          Okkhor Installer' -ForegroundColor Cyan
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ''

# ---------------------------------------------------------------------------
# Require Administrator
# ---------------------------------------------------------------------------

if (-not (Require-Administrator)) {

    # When running through "irm | iex", there is no convenient script file
    # that can simply be relaunched with -File. In that case, download this
    # script to a temporary file and relaunch it elevated.

    $scriptPath = $MyInvocation.MyCommand.Path

    if (-not $scriptPath) {

        Write-Step 'Preparing administrator installation...'

        $tempScript = Join-Path `
            $env:TEMP `
            'okkhor-install.ps1'

        $installerUrl =
        "https://raw.githubusercontent.com/$GitHubOwner/$GitHubRepo/main/scripts/install.ps1"

        Invoke-WebRequest `
            -Uri $installerUrl `
            -OutFile $tempScript

        Start-ElevatedInstaller `
            -ScriptPath $tempScript `
            -DllPath $Dll
    }

    Start-ElevatedInstaller `
        -ScriptPath $scriptPath `
        -DllPath $Dll
}

# ---------------------------------------------------------------------------
# Determine DLL
# ---------------------------------------------------------------------------

if ($Dll) {

    Write-Step 'Using local Okkhor DLL...'

    if (-not (Test-Path -LiteralPath $Dll -PathType Leaf)) {
        throw "DLL not found: $Dll"
    }

    $dllPath = (Resolve-Path -LiteralPath $Dll).Path

    Write-Host "DLL: $dllPath"

}
else {

    Write-Step 'Downloading Okkhor...'

    # -----------------------------------------------------------------------
    # Detect architecture
    # -----------------------------------------------------------------------

    if (-not [Environment]::Is64BitOperatingSystem) {
        throw 'Okkhor currently requires 64-bit Windows.'
    }

    Write-Host 'Architecture: x64'

    # -----------------------------------------------------------------------
    # Query latest GitHub release
    # -----------------------------------------------------------------------

    $releaseApi =
    "https://api.github.com/repos/$GitHubOwner/$GitHubRepo/releases/latest"

    Write-Host 'Checking latest stable release...'

    $release = Invoke-RestMethod `
        -Uri $releaseApi `
        -Headers @{
        'User-Agent' = 'Okkhor-Installer'
    }

    if (-not $release.assets) {
        throw 'The latest Okkhor release does not contain any downloadable assets.'
    }

    $asset = $release.assets |
    Where-Object {
        $_.name -eq 'okkhor-windows-x64.zip'
    } |
    Select-Object -First 1

    if (-not $asset) {
        throw 'The latest Okkhor release does not contain okkhor-windows-x64.zip.'
    }

    Write-Host "Release: $($release.tag_name)"
    Write-Host "Asset:   $($asset.name)"

    # -----------------------------------------------------------------------
    # Download
    # -----------------------------------------------------------------------

    $tempRoot = Join-Path `
        $env:TEMP `
    ("okkhor-" + [Guid]::NewGuid().ToString('N'))

    New-Item `
        -ItemType Directory `
        -Path $tempRoot `
        -Force |
    Out-Null

    $zipPath = Join-Path `
        $tempRoot `
        $asset.name

    $extractDir = Join-Path `
        $tempRoot `
        'extracted'

    New-Item `
        -ItemType Directory `
        -Path $extractDir `
        -Force |
    Out-Null

    try {

        Write-Host 'Downloading release...'

        Invoke-WebRequest `
            -Uri $asset.browser_download_url `
            -OutFile $zipPath

        Write-Host 'Extracting release...'

        Expand-Archive `
            -LiteralPath $zipPath `
            -DestinationPath $extractDir `
            -Force

        $downloadedDll = Get-ChildItem `
            -Path $extractDir `
            -Filter 'okkhor_tsf.dll' `
            -File `
            -Recurse |
        Select-Object -First 1

        if (-not $downloadedDll) {
            throw 'The downloaded Okkhor package does not contain okkhor_tsf.dll.'
        }

        # -------------------------------------------------------------------
        # Install files
        # -------------------------------------------------------------------

        Write-Step 'Installing Okkhor...'

        New-Item `
            -ItemType Directory `
            -Path $InstallDir `
            -Force |
        Out-Null

        $dllPath = Join-Path `
            $InstallDir `
            'okkhor_tsf.dll'

        Copy-Item `
            -LiteralPath $downloadedDll.FullName `
            -Destination $dllPath `
            -Force

        Write-Host "Installed to: $InstallDir"

    }
    finally {

        if (Test-Path -LiteralPath $tempRoot) {
            Remove-Item `
                -LiteralPath $tempRoot `
                -Recurse `
                -Force `
                -ErrorAction SilentlyContinue
        }
    }
}

# ---------------------------------------------------------------------------
# Stop processes that may hold the TSF DLL
# ---------------------------------------------------------------------------

Write-Step 'Stopping Windows components...'

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

$processNames =@(Get-OkkhorProcesses)

foreach ($name in $processNames) {

    $processes = Get-Process `
        -Name $name `
        -ErrorAction SilentlyContinue

    foreach ($process in $processes) {

        Write-Host `
            "  Stopping $name (PID $($process.Id))..."

        try {

            Stop-Process `
                -Id $process.Id `
                -Force `
                -ErrorAction Stop

        }
        catch {

            Write-Warning `
                "Could not stop $name (PID $($process.Id))."
        }
    }
}

Start-Sleep -Milliseconds 500

# ---------------------------------------------------------------------------
# Unregister previous installation
# ---------------------------------------------------------------------------

Write-Step 'Removing previous Okkhor registration...'

$regsvr32 = Join-Path `
    $env:WINDIR `
    'System32\regsvr32.exe'

if (-not (Test-Path -LiteralPath $regsvr32)) {
    throw "regsvr32.exe not found: $regsvr32"
}

if (Test-Path -LiteralPath $dllPath) {

    & $regsvr32 /u /s $dllPath

    # Ignore the result here because this may be the first installation.
}

# ---------------------------------------------------------------------------
# Register new installation
# ---------------------------------------------------------------------------

Write-Step 'Registering Okkhor...'

Write-Host "DLL: $dllPath"

$regsvrProcess = Start-Process `
    -FilePath $regsvr32 `
    -ArgumentList @('/s', $dllPath) `
    -Wait `
    -PassThru

$regsvrExitCode = $regsvrProcess.ExitCode

Write-Host "regsvr32 exit code: $regsvrExitCode"

if ($regsvrExitCode -eq 0) {

    Write-Host `
        'Registration successful.' `
        -ForegroundColor Green
}
else {

    Write-Warning `
        "regsvr32 returned exit code $regsvrExitCode."

    Write-Warning `
        'Verify the Okkhor keyboard appears in Windows before continuing.'
}

# ---------------------------------------------------------------------------
# Restart Explorer
# ---------------------------------------------------------------------------

# Write-Step 'Restarting Windows Explorer...'

# Start-Process explorer.exe -WindowStyle Hidden

# ---------------------------------------------------------------------------
# Cleanup temporary elevated installer
# ---------------------------------------------------------------------------

if (
    $MyInvocation.MyCommand.Path -and
    $MyInvocation.MyCommand.Path -like "$env:TEMP\okkhor-install.ps1"
) {

    $temporaryScript = $MyInvocation.MyCommand.Path

    Start-Job {
        param($Path)

        Start-Sleep -Seconds 2

        Remove-Item `
            -LiteralPath $Path `
            -Force `
            -ErrorAction SilentlyContinue

    } -ArgumentList $temporaryScript |
    Out-Null
}

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------

Write-Host ''
Write-Host '========================================' -ForegroundColor Green
Write-Host '   Okkhor installed successfully!' -ForegroundColor Green
Write-Host '========================================' -ForegroundColor Green
Write-Host ''

Write-Host 'To enable Okkhor Phonetic:'
Write-Host ''
Write-Host 'Settings'
Write-Host '  → Time & language'
Write-Host '  → Language & region'
Write-Host '  → Bangla (Bangladesh)'
Write-Host '  → Language options'
Write-Host '  → Keyboards'
Write-Host '  → Add a keyboard'
Write-Host '  → Okkhor Phonetic'
Write-Host ''

Write-Host 'After adding it, select "Okkhor Phonetic" from the Windows keyboard selector.'
Write-Host ''

Write-Host 'Installation complete.' -ForegroundColor Green
Write-Host ''
Write-Host 'Press any key to exit...'
$null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')