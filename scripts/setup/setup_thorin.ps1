$CURRENT = Get-Location

# Clone or update if necessary
If (!(Test-Path -Path "thorin")) {
    & $GIT_BIN clone --depth 1 --branch $Config.THORIN.BRANCH $Config.THORIN.GIT thorin
    Set-Location "thorin/"
} else {
    Set-Location "thorin/"
    & $GIT_BIN pull
}

$HALF = "$DEPS_ROOT\half".Replace("\", "/").Replace(" ", "` ")
$LLVM = "$DEPS_ROOT\llvm-install".Replace("\", "/").Replace(" ", "` ")

# Check for some possible mistakes beforehand

If (!(Test-Path -Path "$HALF")) {
    throw 'The half directory is not valid'
}

If (!(Test-Path -Path "$LLVM")) {
    throw 'The LLVM directory is not valid'
}

$BUILD_TYPE = $Config.THORIN.BUILD_TYPE

# Setup cmake
$CMAKE_Args = @()
$CMAKE_Args += $Config.CMAKE.EXTRA_ARGS
$CMAKE_Args += $Config.THORIN.EXTRA_ARGS
$CMAKE_Args += '-DCMAKE_BUILD_TYPE:STRING=' + $BUILD_TYPE
$CMAKE_Args += '-DBUILD_SHARED_LIBS:BOOL=OFF' # No need for this
$CMAKE_Args += '-DHalf_DIR:PATH=' + $HALF
$CMAKE_Args += '-DHalf_INCLUDE_DIR:PATH=' + "$HALF/include"
$CMAKE_Args += '-DLLVM_DIR:PATH=' + "$LLVM/lib/cmake/llvm"
$CMAKE_Args += '-DTHORIN_ENABLE_CHECKS:BOOL=ON'
$CMAKE_Args += '-DTHORIN_PROFILE:BOOL=OFF'
$CMAKE_Args += '-DCMAKE_REQUIRE_FIND_PACKAGE_LLVM:BOOL=ON' # We really want JIT

& $CMAKE_BIN -S . -B build $CMAKE_Args 

if ($LASTEXITCODE -ne 0) {
    throw "Failed to configure Thorin"
}

# Build it
& $CMAKE_BIN --build build --config "$BUILD_TYPE" --parallel $Config.CMAKE.PARALLEL_JOBS

if ($LASTEXITCODE -ne 0) {
    throw "Failed to build Thorin"
}

# Install binaries
#Copy-Item -Path "build/bin/*" -Destination "$BIN_ROOT\" > $null

Set-Location $CURRENT
