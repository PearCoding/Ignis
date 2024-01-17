param (
    [string]$config_file="$PSScriptRoot\config.json"
)

$CURRENT=Get-Location

# Get settings
$Config=Get-Content -Path $config_file | ConvertFrom-Json
echo "Config:"
echo $Config

$GIT_BIN=$Config.GIT_BIN
$CMAKE_BIN=$Config.CMAKE_BIN

# Create directory structure
cd $PSScriptRoot\..\..\..\
$IGNIS_ROOT="$(Get-Location)\"

If($Config.LOCATION) {
    If (!(Test-Path "$($Config.LOCATION)")) {
        md "$($Config.LOCATION)" > $null
    }
    cd "$($Config.LOCATION)"
} Else {
    cd ..
    If (!(Test-Path "deps")) {
        md "deps" > $null
    }
    cd "deps"
}
$DEPS_ROOT=Get-Location

# Setup build dir
$BUILD_DIR=$Config.BUILD_DIR
if (![System.IO.Path]::IsPathRooted($BUILD_DIR)) {
    $BUILD_DIR=$(Join-Path -Path "$IGNIS_ROOT" -ChildPath "$BUILD_DIR")
}

# Create some necessary folders
If (!(Test-Path "bin")) {
    md "bin" > $null
}
$BIN_ROOT="$DEPS_ROOT\bin"

If (!(Test-Path "tmp")) {
    md "tmp" > $null
}
$TMP_ROOT="$DEPS_ROOT\tmp"

# Ask for permissions first
If($Config.GET_AnyDSL) {
    $choices = New-Object Collections.ObjectModel.Collection[Management.Automation.Host.ChoiceDescription]
    $choices.Add((New-Object Management.Automation.Host.ChoiceDescription -ArgumentList '&Yes'))
    $choices.Add((New-Object Management.Automation.Host.ChoiceDescription -ArgumentList '&No'))
    $decision = $Host.UI.PromptForChoice("Automatic Ignis Windows Setup",
                                        "The setup script will download and compile AnyDSL. This may take some time and will use a significant amount of CPU power. Are you sure you want to proceed?",
                                        $choices, 0)
    if ($decision -eq 1) {
        throw "AnyDSL setup rejected by user"
    }
}

# Setup dev environment
if(!$Config.SKIP_VC_ENV) {
    & $PSScriptRoot\vc_dev_env.ps1
}

If($Config.GET_ZLIB) {
    & $PSScriptRoot\setup_zlib.ps1
}

If($Config.GET_AnyDSL) {
    & $PSScriptRoot\setup_anydsl.ps1
}

If($Config.GET_TBB) {
    & $PSScriptRoot\setup_tbb.ps1
}

If($Config.GET_SDL2) {
    & $PSScriptRoot\setup_sdl2.ps1
}

If($Config.GET_OIDN) {
    & $PSScriptRoot\setup_oidn.ps1
}

if($Config.CONFIGURE_IGNIS) {
    & $PSScriptRoot\setup_ignis.ps1
}

cd $CURRENT
