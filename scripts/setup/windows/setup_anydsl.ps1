$CURRENT=Get-Location

# Some predefined locations
$ZLIB="$DEPS_ROOT\zlib"
$CUDA=$Env:CUDA_PATH
$CUDAToolkit_NVVM_LIBRARY="$CUDA\nvvm\lib\x64\nvvm.lib".Replace("\", "/")
$ZLIB_LIBRARY="$ZLIB\lib\zlib.lib".Replace("\", "/")
$ZLIB_INCLUDE_DIR="$ZLIB\include".Replace("\", "/")

# Check for some possible mistakes beforehand
If (!(Test-Path $ZLIB)) {
    throw 'The zlib directory is not valid'
}
If (!(Test-Path $CUDA)) {
    Write-Error 'The CUDA directory is not valid. Proceeding will install without NVidia GPU support'
}

# Clone or update if necessary
If (!(Test-Path "anydsl")) {
    & $GIT_BIN clone --branch cmake-based-setup https://github.com/AnyDSL/anydsl.git
    cd "anydsl"
} Else {
    cd "anydsl"
    & $GIT_BIN pull
}

If (!(Test-Path "build")) {
    md "build" > $null
}
cd "build"

$BUILD_TYPE=$Config.AnyDSL_BUILD_TYPE
# Setup cmake
& $CMAKE_BIN $Config.CMAKE_EXTRA_ARGS `
    -DRUNTIME_JIT:BOOL=ON `
    -DCMAKE_BUILD_TYPE:STRING="$BUILD_TYPE" `
    -DAnyDSL_runtime_BUILD_SHARED:BOOL=ON `
    -DAnyDSL_PKG_LLVM_AUTOBUILD:BOOL=ON `
    -DAnyDSL_PKG_LLVM_VERSION:STRING="$($Config.AnyDSL_LLVM_VERSION)" `
    -DAnyDSL_PKG_RV_TAG:STRING="$($Config.AnyDSL_RV_TAG)" `
    -DAnyDSL_PKG_LLVM_URL:STRING="$($Config.AnyDSL_LLVM_URL)" `
    -DLLVM_TARGETS_TO_BUILD:STRING="$($Config.AnyDSL_LLVM_TARGETS)" `
    -DLLVM_ENABLE_PROJECTS:STRING="clang;lld" `
    -DLLVM_ENABLE_BINDINGS:BOOL=OFF `
    -DTHORIN_PROFILE:BOOL=OFF `
    -DCUDAToolkit_NVVM_LIBRARY:PATH="$CUDAToolkit_NVVM_LIBRARY" `
    -DZLIB_LIBRARY:PATH="$ZLIB_LIBRARY" `
    -DZLIB_INCLUDE_DIR:PATH="$ZLIB_INCLUDE_DIR" `
    ..

# Build it
& $CMAKE_BIN --build . --config "$BUILD_TYPE" --target pull-thorin pull-artic pull-runtime runtime runtime_jit_artic artic

If (Test-Path "bin\$BUILD_TYPE\") {
    $ANYDSL_BIN_PATH="bin\$BUILD_TYPE\"
    $LLVM_BIN_PATH="_deps\llvm-build\$BUILD_TYPE\bin\"
} Else {
    $ANYDSL_BIN_PATH="bin\"
    $LLVM_BIN_PATH="_deps\llvm-build\bin\"
}

# Copy necessary stuff
$ZLIB_DLL="$ZLIB\bin\zlib.dll"
If (!(Test-Path $ZLIB_DLL)) {
    Write-Error 'The zlib dll could not be copied. You might have to copy it yourself'
} Else {
    cp $ZLIB_DLL "$ANYDSL_BIN_PATH" > $null
    cp $ZLIB_DLL "$LLVM_BIN_PATH" > $null
}

If (Test-Path $CUDA) {
    cp "$CUDA\nvvm\bin\nvvm*.dll" "$ANYDSL_BIN_PATH" > $null
}

# Copy to parent dir if necessary
Copy-Item -Exclude "artic.exe" -Path ".\$ANYDSL_BIN_PATH*" -Destination "$BIN_ROOT\" > $null

cd $CURRENT
