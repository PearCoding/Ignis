$CURRENT=Get-Location

# Some predefined locations
$ZLIB="$DEPS_ROOT\zlib"
$CUDA=$Env:CUDA_PATH
$CUDAToolkit_NVVM_LIBRARY="$CUDA\nvvm\lib\x64\nvvm.lib".Replace("\", "/")
$ZLIB_LIBRARY="$ZLIB\lib\zlib.lib".Replace("\", "/")
$ZLIB_INCLUDE_DIR="$ZLIB\include".Replace("\", "/")

# Check for some possible mistakes beforehand
If (!(test-path $ZLIB)) {
    throw 'The zlib directory is not valid'
}
If (!(test-path $CUDA)) {
    Write-Error 'The CUDA directory is not valid. Proceeding will install without NVidia GPU support'
}

# Clone or update if necessary
If (!(test-path "anydsl")) {
    & $GIT_BIN clone --branch cmake-based-setup https://github.com/AnyDSL/anydsl.git
    cd "anydsl"
} Else {
    cd "anydsl"
    & $GIT_BIN pull
}

If (!(test-path "build")) {
    md "build"
}
cd "build"

$BUILD_TYPE=$Config.AnyDSL_BUILD_TYPE
# Setup cmake
& $CMAKE_BIN $Config.CMAKE_EXTRA_ARGS `
    -DRUNTIME_JIT=ON `
    -DBUILD_SHARED_LIBS=ON `
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" `
    -DAnyDSL_runtime_BUILD_SHARED=ON `
    -DAnyDSL_PKG_LLVM_AUTOBUILD=ON `
    -DAnyDSL_PKG_LLVM_VERSION="$($Config.AnyDSL_LLVM_VERSION)" `
    -DAnyDSL_PKG_RV_TAG="$($Config.AnyDSL_RV_TAG)" `
    -DAnyDSL_PKG_LLVM_URL="$($Config.AnyDSL_LLVM_URL)" `
    -DTHORIN_PROFILE=OFF `
    -DBUILD_SHARED_LIBS=OFF `
    -DCUDAToolkit_NVVM_LIBRARY="$CUDAToolkit_NVVM_LIBRARY" `
    -DZLIB_LIBRARY="$ZLIB_LIBRARY" `
    -DZLIB_INCLUDE_DIR="$ZLIB_INCLUDE_DIR" `
    ..

# Build it
& $CMAKE_BIN --build . --config "$BUILD_TYPE" --target pull-thorin pull-artic pull-runtime runtime runtime_jit_artic artic

# Copy necessary stuff
$ZLIB_DLL="$ZLIB\bin\zlib.dll"
If (!(test-path $ZLIB_DLL)) {
    Write-Error 'The zlib dll could not be copied. You might have to copy it yourself'
} Else {
    cp $ZLIB_DLL "bin\$BUILD_TYPE\"
    cp $ZLIB_DLL "_deps\llvm-build\$BUILD_TYPE\bin\"
}

If (!(test-path $CUDA)) {
    cp "$CUDA\nvvm\bin\nvvm*.dll" "bin\$BUILD_TYPE\"
}

# Copy to parent dir if necessary
Copy-Item -Exclude "artic.exe" -Path ".\bin\$BUILD_TYPE\*" -Destination "$BIN_ROOT\"

cd $CURRENT
