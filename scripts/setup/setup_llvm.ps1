$CURRENT = Get-Location

# TODO: RV

if (!$Config.LLVM.FORCE && (Test-Path -Path 'llvm-install/bin/lld*')) {
    Write-Host "Skipping LLVM as it seems to be already installed. Use LLVM.FORCE=true to proceed with LLVM."
    return
}

# Ask for permissions first
{
    $choices = New-Object Collections.ObjectModel.Collection[Management.Automation.Host.ChoiceDescription]
    $choices.Add((New-Object Management.Automation.Host.ChoiceDescription -ArgumentList '&Yes'))
    $choices.Add((New-Object Management.Automation.Host.ChoiceDescription -ArgumentList '&No'))
    $decision = $Host.UI.PromptForChoice("Automatic Ignis Setup",
                                        "The setup script will download and compile LLVM. This may take some time and will use a significant amount of CPU power. Are you sure you want to proceed?",
                                        $choices, 0)
    if ($decision -eq 1) {
        throw "LLVM setup rejected by user"
    }
}

# Clone or update if necessary
If (!(Test-Path -Path "llvm-src")) {
    & $GIT_BIN clone --depth 1 --branch $Config.LLVM.BRANCH $Config.LLVM.GIT llvm-src
}

Set-Location "llvm-src/"
& $GIT_BIN apply --directory llvm --ignore-space-change --ignore-whitespace $IGNIS_ROOT/scripts/setup/nvptx_feature.patch

$BUILD_TYPE = $Config.LLVM.BUILD_TYPE
$LLVM_ROOT = "$DEPS_ROOT\llvm-install".Replace("\", "/")

# Setup cmake
$CMAKE_Args = @()
$CMAKE_Args += $Config.CMAKE.EXTRA_ARGS
$CMAKE_Args += $Config.LLVM.EXTRA_ARGS
$CMAKE_Args += '-DCMAKE_BUILD_TYPE:STRING=' + $BUILD_TYPE
$CMAKE_Args += '-DCMAKE_INSTALL_PREFIX:PATH=' + $LLVM_ROOT
$CMAKE_Args += '-DCMAKE_INSTALL_MESSAGE=LAZY'
$CMAKE_Args += '-DLLVM_TARGETS_TO_BUILD:STRING=' + $Config.LLVM.TARGETS
$CMAKE_Args += '-DLLVM_ENABLE_BINDINGS:BOOL=OFF'
$CMAKE_Args += '-DLLVM_ENABLE_PROJECTS:STRING=' + "clang;lld"
$CMAKE_Args += '-DLLVM_INCLUDE_TESTS:BOOL=OFF'
$CMAKE_Args += '-DLLVM_ENABLE_RTTI:BOOL=ON'

& $CMAKE_BIN -S llvm -B build $CMAKE_Args 

if ($LASTEXITCODE -ne 0) {
    throw "Failed to configure LLVM"
}

# Build it
& $CMAKE_BIN --build build --config "$BUILD_TYPE" --parallel $Config.CMAKE.PARALLEL_JOBS

if ($LASTEXITCODE -ne 0) {
    throw "Failed to build LLVM"
}

# Install it
& $CMAKE_BIN --install build --config "$BUILD_TYPE"

if ($LASTEXITCODE -ne 0) {
    throw "Failed to install LLVM"
}

Set-Location $CURRENT
