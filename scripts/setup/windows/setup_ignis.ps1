$CURRENT=Get-Location

If (!(test-path "$BUILD_DIR")) {
    md "$BUILD_DIR"
}
cd "$BUILD_DIR"

$CLANG_BIN="$DEPS_ROOT\anydsl\build\_deps\llvm-build\$($Config.AnyDSL_BUILD_TYPE)\bin\clang.exe".Replace("\", "/")
$RUNTIME_DIR="$DEPS_ROOT\anydsl\build\share\anydsl\cmake".Replace("\", "/")
$ARTIC_BIN_DIR="$DEPS_ROOT\anydsl\build\bin\$($Config.AnyDSL_BUILD_TYPE)".Replace("\", "/")
$ARTIC_BIN="$ARTIC_BIN_DIR\artic.exe".Replace("\", "/")
$TBB_LIB="$DEPS_ROOT\tbb\lib\intel64\vc14\tbb12.lib".Replace("\", "/")
$TBB_MALLOC_LIB="$DEPS_ROOT\tbb\lib\intel64\vc14\tbbmalloc.lib".Replace("\", "/")
$TBB_INCLUDE="$DEPS_ROOT\tbb\include".Replace("\", "/")
$ZLIB_LIB="$DEPS_ROOT\zlib\lib\zlib.lib".Replace("\", "/")
$ZLIB_INCLUDE="$DEPS_ROOT\zlib\include".Replace("\", "/")
$SDL2_LIB="$DEPS_ROOT\SDL2\lib\x64\SDL2.lib".Replace("\", "/")
$SDL2_INCLUDE="$DEPS_ROOT\SDL2\include".Replace("\", "/")

If($Config.CMAKE_SLN) {
    cmake -DCMAKE_BUILD_TYPE="Release" `
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
    "$IGNIS_ROOT"

    # Make sure all the dlls are in the correct place (for Release at least)
    $OUTPUT_DIR="$BUILD_DIR\bin"
    if(!(Test-Path "$OUTPUT_DIR\Release")) {
        md "$OUTPUT_DIR\Release"
    }

    cp "$BIN_ROOT\*" "$OUTPUT_DIR\Release"
} Else {
    cmake -GNinja -DCMAKE_BUILD_TYPE="Release" `
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
    "$IGNIS_ROOT"
    
    # Make sure all the dlls are in the correct place (for Release at least)
    $OUTPUT_DIR="$BUILD_DIR\bin"
    if(!(Test-Path "$OUTPUT_DIR")) {
        md "$OUTPUT_DIR"
    }

    cp "$BIN_ROOT\*" "$OUTPUT_DIR"
}

cd $CURRENT