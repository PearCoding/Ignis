param (
    [string]$config_file="$PSScriptRoot\config.json"
)

# Compiliance with old powershell
if (($null -eq $IsWindows) -or ($PSVersionTable.PSVersion.Major -lt 6)) {
    Write-Warning "Using old Powershell! Script might not work."
    $IsWindows = $true
    $IsLinux = $false
    $IsMacOS = $false
}

$CURRENT=Get-Location

# Get settings
$Config=Get-Content -Path $config_file | ConvertFrom-Json

if($null -eq $Config.CMAKE.BINARY) {
    $Config.CMAKE.BINARY = "cmake"
}

if($null -eq $Config.GIT.BINARY) {
    $Config.GIT.BINARY = "git"
}

if($null -eq $Config.CMAKE.PARALLEL_JOBS) {
    $Config.CMAKE.PARALLEL_JOBS = 0
}

if($Config.CMAKE.PARALLEL_JOBS -le 0) {
    $Config.CMAKE.PARALLEL_JOBS = [System.Environment].ProcessorCount
}

Write-Output "Config:"
Write-Output $Config

$GIT_BIN=$Config.GIT.BINARY
$CMAKE_BIN=$Config.CMAKE.BINARY

# Create directory structure
Set-Location $PSScriptRoot\..\..\
. "$PSScriptRoot/utils.ps1"

$IGNIS_ROOT="$(Get-Location)\"

If($Config.LOCATION) {
    If (!(Test-Path "$($Config.LOCATION)")) {
        mkdir "$($Config.LOCATION)" > $null
    }
    Set-Location "$($Config.LOCATION)"
} Else {
    Set-Location ..
    If (!(Test-Path "deps")) {
        mkdir "deps" > $null
    }
    Set-Location "deps"
}
$DEPS_ROOT=Get-Location

# Setup build dir
$BUILD_DIR=$Config.BUILD_DIR
if (![System.IO.Path]::IsPathRooted($BUILD_DIR)) {
    $BUILD_DIR=$(Join-Path -Path "$IGNIS_ROOT" -ChildPath "$BUILD_DIR")
}

# Create some necessary folders
If (!(Test-Path "bin")) {
    mkdir "bin" > $null
}
$BIN_ROOT="$DEPS_ROOT\bin"

If (!(Test-Path "tmp")) {
    mkdir "tmp" > $null
}
$TMP_ROOT="$DEPS_ROOT\tmp"

# Setup dev environment
try {
    if($IsWindows -and (!(GetPD $Config.SKIP_VC_ENV $false))) {
        & $PSScriptRoot\vc_dev_env.ps1
    }

    If($Config.ZLIB -and (GetPD $Config.ZLIB.ENABLED $true)){
        & $PSScriptRoot\setup_zlib.ps1
    }

    If($Config.TBB -and (GetPD $Config.TBB.ENABLED $true)) {
        & $PSScriptRoot\setup_tbb.ps1
    }

    If($Config.HALF -and (GetPD $Config.HALF.ENABLED $true)) {
        & $PSScriptRoot\setup_half.ps1
    }

    If($Config.LLVM -and (GetPD $Config.LLVM.ENABLED $true)) {
        & $PSScriptRoot\setup_llvm.ps1
    }

    If($Config.THORIN -and (GetPD $Config.THORIN.ENABLED $true)) {
        & $PSScriptRoot\setup_thorin.ps1
    }

    If($Config.ARTIC -and (GetPD $Config.ARTIC.ENABLED $true)) {
        & $PSScriptRoot\setup_artic.ps1
    }

    If($Config.RUNTIME -and (GetPD $Config.RUNTIME.ENABLED $true)) {
        & $PSScriptRoot\setup_runtime.ps1
    }

    If($Config.SDL2 -and (GetPD $Config.SDL2.ENABLED $true)) {
        & $PSScriptRoot\setup_sdl2.ps1
    }

    If($Config.OIDN -and (GetPD $Config.OIDN.ENABLED $true)) {
        & $PSScriptRoot\setup_oidn.ps1
    }

    if($Config.IGNIS -and (GetPD $Config.IGNIS.ENABLED $true)) {
        & $PSScriptRoot\setup_ignis.ps1
    }
} catch {
    Write-Host "Setup failed:" -ForegroundColor red
    Write-Host $_ -ForegroundColor red
}

Set-Location $CURRENT
