$CURRENT = Get-Location

$BUILD_DIR = $Config.IGNIS.BUILD_DIR.Replace("{BUILD_TYPE}", $Config.IGNIS.BUILD_TYPE)

Set-Location $IGNIS_ROOT

$LLVM_ROOT = (GetLLVMRoot).Replace("\", "/")
$CLANG_BIN_DIR = "$LLVM_ROOT/bin"

# Locate a clang binary for the AnyDSL toolchain: prefer the one next to the
# selected LLVM, then a versioned clang on the PATH. It is only invoked for the
# optional AOT object output (Ignis emits just the C interface), so the exact
# version is not critical; an empty result lets the runtime config search itself.
if ($IsWindows) { $clang_exe = "clang.exe" } else { $clang_exe = "clang" }
$CLANG_BIN = ""
$clang_candidates = @("$CLANG_BIN_DIR/$clang_exe")
if (!$IsWindows) {
    foreach ($v in 20, 19, 18, 17) { $clang_candidates += "$CLANG_BIN_DIR/clang-$v" }
}
foreach ($c in $clang_candidates) {
    if (Test-Path -Path $c) { $CLANG_BIN = $c.Replace("\", "/"); break }
}
if ([string]::IsNullOrEmpty($CLANG_BIN)) {
    foreach ($name in @($clang_exe, "clang-20", "clang-19", "clang-18", "clang-17")) {
        $found = Get-Command $name -ErrorAction SilentlyContinue
        if ($found) { $CLANG_BIN = $found.Source.Replace("\", "/"); break }
    }
}

$ARTIC_BIN_DIR = $BIN_ROOT
if ($IsWindows) {
    $ARTIC_BIN = "$BIN_ROOT\artic.exe".Replace("\", "/")
} else {
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
if (![string]::IsNullOrEmpty($CLANG_BIN)) {
    $CMAKE_Args += '-DClang_BIN:FILEPATH=' + $CLANG_BIN
}
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
$CMAKE_Args += '-DBUILD_TESTING:BOOL=ON'


foreach ($device in $Config.RUNTIME.DEVICES) {
    $runtime_name = "runtime_$device"
    $runtime_build_dir = "$DEPS_ROOT\runtime\build_$device"
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
        & $Config.CPACK.BINARY -G NSIS -C "$BUILD_TYPE"
    }
    elseif ($IsLinux) {
        & $Config.CPACK.BINARY -G DEB -C "$BUILD_TYPE"
    }
    else {
        & $Config.CPACK.BINARY -G ZIP -C "$BUILD_TYPE"
    }
}

if ($LASTEXITCODE -ne 0) {
    throw "Failed to build Ignis installer"
}

Set-Location $CURRENT
