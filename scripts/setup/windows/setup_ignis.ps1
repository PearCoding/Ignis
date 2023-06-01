$CURRENT=Get-Location

If (!(Test-Path "$BUILD_DIR")) {
    md "$BUILD_DIR" > $null
}
cd "$BUILD_DIR"

$CLANG_BIN_DIR="$DEPS_ROOT\anydsl\build\_deps\llvm-build\$($Config.AnyDSL_BUILD_TYPE)\bin"
if (Test-Path "$CLANG_BIN_DIR") {
    $CLANG_BIN_DIR=$CLANG_BIN_DIR.Replace("\", "/")
} Else {
    $CLANG_BIN_DIR="$DEPS_ROOT\anydsl\build\_deps\llvm-build\bin".Replace("\", "/")
}
$CLANG_BIN="$CLANG_BIN_DIR\clang.exe".Replace("\", "/")

$ARTIC_BIN_DIR="$DEPS_ROOT\anydsl\build\bin\$($Config.AnyDSL_BUILD_TYPE)"
if (Test-Path "$ARTIC_BIN_DIR") {
    $ARTIC_BIN_DIR=$ARTIC_BIN_DIR.Replace("\", "/")
} Else {
    $ARTIC_BIN_DIR="$DEPS_ROOT\anydsl\build\bin".Replace("\", "/")
}
$ARTIC_BIN="$ARTIC_BIN_DIR\artic.exe".Replace("\", "/")

$RUNTIME_DIR="$DEPS_ROOT\anydsl\build\share\anydsl\cmake".Replace("\", "/")
$TBB_LIB="$DEPS_ROOT\tbb\lib\intel64\vc14\tbb12.lib".Replace("\", "/")
$TBB_MALLOC_LIB="$DEPS_ROOT\tbb\lib\intel64\vc14\tbbmalloc.lib".Replace("\", "/")
$TBB_INCLUDE="$DEPS_ROOT\tbb\include".Replace("\", "/")
$ZLIB_LIB="$DEPS_ROOT\zlib\lib\zlib.lib".Replace("\", "/")
$ZLIB_INCLUDE="$DEPS_ROOT\zlib\include".Replace("\", "/")
$SDL2_LIB="$DEPS_ROOT\SDL2\lib\x64\SDL2.lib".Replace("\", "/")
$SDL2_INCLUDE="$DEPS_ROOT\SDL2\include".Replace("\", "/")

& $CMAKE_BIN $Config.CMAKE_EXTRA_ARGS -DCMAKE_BUILD_TYPE="Release" `
    -DBUILD_TESTING=OFF `
    -DFETCHCONTENT_UPDATES_DISCONNECTED=ON `
    -DClang_BIN="$CLANG_BIN" `
    -DAnyDSL_runtime_DIR="$RUNTIME_DIR" `
    -DArtic_BINARY_DIR="$ARTIC_BIN_DIR" `
    -DArtic_BIN="$ARTIC_BIN" `
    -DTBB_tbb_LIBRARY_RELEASE="$TBB_LIB" `
    -DTBB_tbbmalloc_LIBRARY_RELEASE="$TBB_MALLOC_LIB" `
    -DTBB_INCLUDE_DIR="$TBB_INCLUDE" `
    -DZLIB_LIBRARY="$ZLIB_LIB" `
    -DZLIB_INCLUDE_DIR="$ZLIB_INCLUDE" `
    -DSDL2_LIBRARY="$SDL2_LIB" `
    -DSDL2_INCLUDE_DIR="$SDL2_INCLUDE" `
    -DOpenImageDenoise_DIR="$DEPS_ROOT/oidn/lib/cmake/OpenImageDenoise-2.0.0" `
    -DIG_WITH_ASSERTS:BOOL=ON `
    "$IGNIS_ROOT"

# Make sure all the dlls are in the correct place (for Release at least)
If(!$Config.CMAKE_EXTRA_ARGS.Contains("-GNinja")) { # TODO: What about other single configuration generators?
    $OUTPUT_DIR="$BUILD_DIR\bin"
    if(!(Test-Path "$OUTPUT_DIR\Release")) {
        md "$OUTPUT_DIR\Release" > $null
    }

    cp "$BIN_ROOT\*" "$OUTPUT_DIR\Release" > $null
} Else {
    $OUTPUT_DIR="$BUILD_DIR\bin"
    if(!(Test-Path "$OUTPUT_DIR")) {
        md "$OUTPUT_DIR" > $null
    }

    cp "$BIN_ROOT\*" "$OUTPUT_DIR" > $null
}

cd $CURRENT
