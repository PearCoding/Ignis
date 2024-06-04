param (
    [string]$config_file="$PSScriptRoot\config.json"
)

$CURRENT=Get-Location

# Get settings
$Config=Get-Content -Path $config_file | ConvertFrom-Json
Write-Output "Config:"
Write-Output $Config

$GIT_BIN=$Config.GIT.BINARY
$CMAKE_BIN=$Config.CMAKE.BINARY

if($Config.CMAKE.PARALLEL_JOBS -le 0) {
    $Config.CMAKE.PARALLEL_JOBS = [System.Environment].ProcessorCount
}

# Create directory structure
Set-Location $PSScriptRoot\..\..\
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
    if($IsWindows && !$Config.SKIP_VC_ENV) {
        & $PSScriptRoot\vc_dev_env.ps1
    }

    If($Config.ZLIB && $Config.ZLIB.ENABLED) {
        & $PSScriptRoot\setup_zlib.ps1
    }

    If($Config.TBB && $Config.TBB.ENABLED) {
        & $PSScriptRoot\setup_tbb.ps1
    }

    If($Config.HALF && $Config.HALF.ENABLED) {
        & $PSScriptRoot\setup_half.ps1
    }

    If($Config.LLVM && $Config.LLVM.ENABLED) {
        & $PSScriptRoot\setup_llvm.ps1
    }

    If($Config.THORIN && $Config.THORIN.ENABLED) {
        & $PSScriptRoot\setup_thorin.ps1
    }

    If($Config.ARTIC && $Config.ARTIC.ENABLED) {
        & $PSScriptRoot\setup_artic.ps1
    }

    If($Config.RUNTIME && $Config.RUNTIME.ENABLED) {
        & $PSScriptRoot\setup_runtime.ps1
    }

    If($Config.SDL2 && $Config.SDL2.ENABLED) {
        & $PSScriptRoot\setup_sdl2.ps1
    }

    If($Config.OIDN && $Config.OIDN.ENABLED) {
        & $PSScriptRoot\setup_oidn.ps1
    }

    if($Config.IGNIS && $Config.IGNIS.ENABLED) {
        & $PSScriptRoot\setup_ignis.ps1
    }
} catch {
    Write-Host "Setup failed:" -ForegroundColor red
    Write-Host $_ -ForegroundColor red
}

Set-Location $CURRENT
