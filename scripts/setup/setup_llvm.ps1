$CURRENT = Get-Location

# TODO: RV

if (UseSystemLLVM) {
    Write-Host "Using system LLVM at $($Config.LLVM.SYSTEM_DIR); skipping LLVM download/build. Set LLVM.SYSTEM_DIR to \"\" or LLVM.FORCE_BUILD=true to build from source."
    return
}

# On Windows, default to a prebuilt LLVM from conda-forge instead of the long
# from-source build. conda-forge's llvmdev is built with the dynamic CRT (/MD),
# which the DLL-based AnyDSL runtime requires. The official llvm.org release
# archives do NOT work here: they are static-CRT (/MT + rpmalloc) builds and
# fail to link into runtime.dll. (Their lack of RTTI is NOT the problem --
# thorin and artic compile and link fine against a no-RTTI LLVM.)
if ($IsWindows -and (GetPD $Config.LLVM.PREBUILT $true) -and (!(GetPD $Config.LLVM.FORCE_BUILD $false))) {
    $CONDA_LLVM = "$DEPS_ROOT\llvm-conda"
    if (Test-Path -Path "$CONDA_LLVM\Library\lib\cmake\llvm") {
        Write-Host "Using prebuilt conda LLVM at $CONDA_LLVM. Set LLVM.PREBUILT=false or LLVM.FORCE_BUILD=true to build from source."
    }
    else {
        $MM_BIN = "$DEPS_ROOT\mm\micromamba.exe"
        if (!(Test-Path -Path $MM_BIN)) {
            if (!(Test-Path -Path "$DEPS_ROOT\mm")) { mkdir "$DEPS_ROOT\mm" > $null }
            Invoke-WebRequest -Uri "https://github.com/mamba-org/micromamba-releases/releases/latest/download/micromamba-win-64" -OutFile $MM_BIN
        }
        $env:MAMBA_ROOT_PREFIX = "$DEPS_ROOT\mm\root"
        $packages = GetPD $Config.LLVM.PREBUILT_PACKAGES @("llvmdev=20.1", "clangdev=20.1", "lld=20.1", "zlib", "zstd", "libxml2-devel")
        & $MM_BIN create -y -p $CONDA_LLVM -c conda-forge @packages
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to provision the prebuilt LLVM via micromamba"
        }
    }
    # Make the conda packages (zlib, zstd, libxml2) visible to every sub-build
    # that re-runs find_package(LLVM), e.g. through thorin.
    $env:CMAKE_PREFIX_PATH = "$CONDA_LLVM\Library;$env:CMAKE_PREFIX_PATH"
    return
}

if ((!(GetPD $Config.LLVM.FORCE $false)) -and (Test-Path -Path 'llvm-install/bin/lld*')) {
    Write-Host "Skipping LLVM as it seems to be already installed. Use LLVM.FORCE=true to proceed with LLVM."
    return
}

# if (!([string]::IsNullOrEmpty($Config.LLVM.DOWNLOAD_URL))) {
#     Set-Location "tmp"
#     if (!(Test-Path -Path 'llvm_install.exe')) {
#         Invoke-WebRequest -UserAgent "Wget" -Uri $Config.LLVM.DOWNLOAD_URL -OutFile "llvm_install.exe"
#     }
#     Start-Process -FilePath .\llvm_install.exe -ArgumentList "/D=$DEPS_ROOT\llvm-install" -Wait
#     Set-Location $CURRENT
#     return
# }

# Ask for permissions first
$choices = New-Object Collections.ObjectModel.Collection[Management.Automation.Host.ChoiceDescription]
$choices.Add((New-Object Management.Automation.Host.ChoiceDescription -ArgumentList '&Yes'))
$choices.Add((New-Object Management.Automation.Host.ChoiceDescription -ArgumentList '&No'))
$decision = $Host.UI.PromptForChoice("Automatic Ignis Setup",
    "The setup script will download and compile LLVM. This may take some time and will use a significant amount of CPU power. Are you sure you want to proceed?",
    $choices, 0)
if ($decision -eq 1) {
    throw "LLVM setup rejected by user"
}

# Clone or update if necessary 
If (!(Test-Path -Path "llvm-src")) {
    & $GIT_BIN clone --depth 1 --branch $Config.LLVM.BRANCH $Config.LLVM.GIT llvm-src
}

Set-Location "llvm-src/"
& $GIT_BIN apply --directory llvm --ignore-space-change --ignore-whitespace "$IGNIS_ROOT/scripts/setup/nvptx_feature.patch".Replace("\", "/")

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
if ($IsLinux) {
    $CMAKE_Args += '-DLLVM_BUILD_LLVM_DYLIB:BOOL=ON'
    $CMAKE_Args += '-DLLVM_LINK_LLVM_DYLIB:BOOL=ON'
}

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
