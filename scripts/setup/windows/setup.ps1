$CURRENT=Get-Location

# Create directory structure
cd $PSScriptRoot\..\..\..\..\
$IGNIS_ROOT="$(Get-Location)\Ignis\"

If (!(test-path "deps")) {
    md "deps"
}
cd "deps"
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

# Setup dev environment
& $PSScriptRoot\vc_dev_env.ps1

& $PSScriptRoot\setup_zlib.ps1
& $PSScriptRoot\setup_anydsl.ps1
& $PSScriptRoot\setup_eigen3.ps1
& $PSScriptRoot\setup_tbb.ps1
& $PSScriptRoot\setup_sdl2.ps1

& $PSScriptRoot\setup_ignis.ps1

cd $CURRENT
