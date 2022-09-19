param (
    [string]$config_file="$PSScriptRoot\config.json"
)

$CURRENT=Get-Location

# Get settings
$Config=Get-Content -Path $config_file | ConvertFrom-Json
echo "Config:"
echo $Config

# Create directory structure
cd $PSScriptRoot\..\..\..\
$IGNIS_ROOT="$(Get-Location)\"

If($Config.LOCATION) {
    If (!(test-path "$($Config.LOCATION)")) {
        md "$($Config.LOCATION)"
    }
    cd "$($Config.LOCATION)"
} Else {
    cd ..
    If (!(test-path "deps")) {
        md "deps"
    }
    cd "deps"
}
$DEPS_ROOT=Get-Location

# Create some necessary folders
If (!(test-path "bin")) {
    md "bin"
}
$BIN_ROOT="$DEPS_ROOT\bin"

If (!(test-path "tmp")) {
    md "tmp"
}
$TMP_ROOT="$DEPS_ROOT\tmp"

# Ask for permissions first
If($Config.GET_AnyDSL) {
    $choices = New-Object Collections.ObjectModel.Collection[Management.Automation.Host.ChoiceDescription]
    $choices.Add((New-Object Management.Automation.Host.ChoiceDescription -ArgumentList '&Yes'))
    $choices.Add((New-Object Management.Automation.Host.ChoiceDescription -ArgumentList '&No'))
    $decision = $Host.UI.PromptForChoice("Automatic Ignis Windows Setup",
                                        "The setup script will download and compile AnyDSL. This may take some time and will use a significant amount of CPU power. Are you Sure You Want To Proceed?",
                                        $choices, 0)
    if ($decision -eq 1) {
        throw "AnyDSL setup rejected by user"
    }
}

# Setup dev environment
& $PSScriptRoot\vc_dev_env.ps1

If($Config.GET_ZLIB) {
    & $PSScriptRoot\setup_zlib.ps1
}

If($Config.GET_AnyDSL) {
    & $PSScriptRoot\setup_anydsl.ps1
}

If($Config.GET_Eigen3) {
    & $PSScriptRoot\setup_eigen3.ps1
}

If($Config.GET_TBB) {
    & $PSScriptRoot\setup_tbb.ps1
}

If($Config.GET_SDL2) {
    & $PSScriptRoot\setup_sdl2.ps1
}

& $PSScriptRoot\setup_ignis.ps1

cd $CURRENT
