$CURRENT = Get-Location

$BUILD_DIR = $Config.IGNIS.BUILD_DIR

Set-Location $IGNIS_ROOT

$CLANG_BIN_DIR = "$DEPS_ROOT\llvm-install\$($Config.AnyDSL_BUILD_TYPE)\bin"
if (Test-Path "$CLANG_BIN_DIR") {
    $CLANG_BIN_DIR = $CLANG_BIN_DIR.Replace("\", "/")
}
Else {
    $CLANG_BIN_DIR = "$DEPS_ROOT\llvm-install\bin".Replace("\", "/")
}

$ARTIC_BIN_DIR = $BIN_ROOT
if ($IsWindows) {
    $ARTIC_BIN = "$BIN_ROOT\artic.exe".Replace("\", "/")
    $CLANG_BIN = "$CLANG_BIN_DIR\clang.exe".Replace("\", "/")
} else {
    $CLANG_BIN = "$CLANG_BIN_DIR\clang".Replace("\", "/")
    $ARTIC_BIN = "$BIN_ROOT\artic".Replace("\", "/")
}

$RUNTIME_DIR = "$DEPS_ROOT\runtime\build\share\anydsl\cmake".Replace("\", "/")

if ($IsWindows) {
    $TBB_DIR = "$DEPS_ROOT\tbb\lib\cmake\tbb".Replace("\", "/")
    $ZLIB_LIB = "$DEPS_ROOT\zlib\lib\zlib.lib".Replace("\", "/")
    $ZLIB_INCLUDE = "$DEPS_ROOT\zlib\include".Replace("\", "/")
    $SDL2_LIB = "$DEPS_ROOT\SDL2\lib\x64\SDL2.lib".Replace("\", "/")
    $SDL2_INCLUDE = "$DEPS_ROOT\SDL2\include".Replace("\", "/")

    $OIDN_DIR = Get-ChildItem -Path "$DEPS_ROOT/oidn/lib/cmake" -Directory | Sort-Object -Descending | Select-Object -First 1
}

$BUILD_TYPE = $Config.Ignis.BUILD_TYPE

$CMAKE_Args = @()
$CMAKE_Args += $Config.CMAKE.EXTRA_ARGS
$CMAKE_Args += $Config.IGNIS.EXTRA_ARGS
$CMAKE_Args += '-DCMAKE_BUILD_TYPE:STRING=' + $BUILD_TYPE
$CMAKE_Args += '-DClang_BIN:FILEPATH=' + $CLANG_BIN
$CMAKE_Args += '-DAnyDSL_runtime_DIR:PATH=' + $RUNTIME_DIR # Default variant
if ($IsWindows) {
    $CMAKE_Args += '-DArtic_BINARY_DIR:PATH=' + $ARTIC_BIN_DIR
    $CMAKE_Args += '-DArtic_BIN:FILEPATH=' + $ARTIC_BIN
    $CMAKE_Args += '-DTBB_DIR:PATH=' + $TBB_DIR
    $CMAKE_Args += '-DZLIB_LIBRARY:FILEPATH=' + $ZLIB_LIB
    $CMAKE_Args += '-DZLIB_INCLUDE_DIR:PATH=' + $ZLIB_INCLUDE
    $CMAKE_Args += '-DSDL2_LIBRARY:FILEPATH=' + $SDL2_LIB
    $CMAKE_Args += '-DSDL2_INCLUDE_DIR:PATH=' + $SDL2_INCLUDE
    $CMAKE_Args += '-DOpenImageDenoise_DIR:PATH=' + $($OIDN_DIR.FullName)
}
$CMAKE_Args += '-DIG_WITH_ASSERTS:BOOL=ON'
$CMAKE_Args += '-DBUILD_TESTING:BOOL=OFF'


foreach ($device in $Config.RUNTIME.DEVICES) {
    $runtime_name = "runtime_$device"
    $runtime_build_dir = "$DEPS_ROOT\runtime\$device"
    if ($IsWindows) {
        $runtime_lib = "$($runtime_name).lib"
        $runtime_jit_lib = "$($runtime_name)_jit_artic.lib"
    } else {
        $runtime_lib = "lib$($runtime_name).so"
        $runtime_jit_lib = "lib$($runtime_name)_jit_artic.so"
    }
    if (Test-Path -Path "$runtime_build_dir/lib/$runtime_lib") {
        $CMAKE_Args += "-DAnyDSLRuntimeDevice_$($device)_INCLUDE_DIR:PATH=" + ($runtime_build_dir + "/../src").Replace("\", "/")
        $CMAKE_Args += "-DAnyDSLRuntimeDevice_$($device)_LIBRARY:FILEPATH=" + ($runtime_build_dir + "/lib/$runtime_lib").Replace("\", "/")
        $CMAKE_Args += "-DAnyDSLRuntimeDevice_$($device)_LIBRARY_JIT:FILEPATH=" + ($runtime_build_dir + "/lib/$runtime_jit_lib").Replace("\", "/")
    }
}

& $CMAKE_BIN -S . -B $BUILD_DIR $CMAKE_Args 

if ($LASTEXITCODE -ne 0) {
    throw "Failed to configure Ignis"
}

# Make sure all the dlls are in the correct place (for Release at least)
If (!$Config.CMAKE.EXTRA_ARGS.Contains("-GNinja")) {
    # TODO: What about other single configuration generators?
    $OUTPUT_DIR = "$BUILD_DIR\bin"
    if (!(Test-Path "$OUTPUT_DIR\$($Config.IGNIS.BUILD_TYPE)")) {
        mkdir "$OUTPUT_DIR\$($Config.IGNIS.BUILD_TYPE)" > $null
    }

    Copy-Item "$BIN_ROOT\*" "$OUTPUT_DIR\$($Config.IGNIS.BUILD_TYPE)" > $null
}
Else {
    $OUTPUT_DIR = "$BUILD_DIR\bin"
    if (!(Test-Path "$OUTPUT_DIR")) {
        mkdir "$OUTPUT_DIR" > $null
    }

    Copy-Item "$BIN_ROOT\*" "$OUTPUT_DIR" > $null
}

if ($Config.IGNIS.SKIP_BUILD) {
    Set-Location $CURRENT
    return
}

& $CMAKE_BIN --build $BUILD_DIR --config "$BUILD_TYPE" --parallel $Config.CMAKE.PARALLEL_JOBS

if ($LASTEXITCODE -ne 0) {
    throw "Failed to build Ignis"
}

if (GetPD $Config.IGNIS.BUILD_INSTALLER $false) {
    Set-Location $BUILD_DIR
    if ($IsWindows) {
        & $Config.CPACK.BINARY -G NSIS --config "$BUILD_TYPE"
    }
    elseif ($IsLinux) {
        & $Config.CPACK.BINARY -G DEB --config "$BUILD_TYPE"
    }
    else {
        & $Config.CPACK.BINARY -G ZIP --config "$BUILD_TYPE"
    }
}

if ($LASTEXITCODE -ne 0) {
    throw "Failed to build Ignis installer"
}

Set-Location $CURRENT
