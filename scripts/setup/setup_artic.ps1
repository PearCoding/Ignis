$CURRENT = Get-Location

# Clone or update if necessary
If (!(Test-Path -Path "artic")) {
    & $GIT_BIN clone --depth 1 --branch $Config.ARTIC.BRANCH $Config.ARTIC.GIT artic
    Set-Location "artic/"
} else {
    Set-Location "artic/"
    & $GIT_BIN pull
}

$THORIN = "$DEPS_ROOT\thorin".Replace("\", "/").Replace(" ", "` ")

# Check for some possible mistakes beforehand

If (!(Test-Path -Path "$THORIN")) {
    throw 'The thorin directory is not valid'
}


$BUILD_TYPE = $Config.ARTIC.BUILD_TYPE

# Setup cmake
$CMAKE_Args = @()
$CMAKE_Args += $Config.CMAKE.EXTRA_ARGS
$CMAKE_Args += $Config.ARTIC.EXTRA_ARGS
$CMAKE_Args += '-DCMAKE_BUILD_TYPE:STRING=' + $BUILD_TYPE
$CMAKE_Args += '-DThorin_DIR:PATH=' + "$THORIN/build/share/anydsl/cmake"
$CMAKE_Args += '-DBUILD_SHARED_LIBS:BOOL=OFF'
$CMAKE_Args += '-DBUILD_TESTING:BOOL=OFF'

& $CMAKE_BIN -S . -B build $CMAKE_Args 

if ($LASTEXITCODE -ne 0) {
    throw "Failed to configure Artic"
}

# Build it
& $CMAKE_BIN --build build --config "$BUILD_TYPE" --parallel $Config.CMAKE.PARALLEL_JOBS

if ($LASTEXITCODE -ne 0) {
    throw "Failed to build Artic"
}

# Install binaries
Copy-Item -Path "build/bin/*" -Destination "$BIN_ROOT\" > $null

Set-Location $CURRENT
