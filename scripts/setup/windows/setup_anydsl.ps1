$CURRENT = Get-Location

# Some predefined locations
$ZLIB = "$DEPS_ROOT\zlib"
$CUDA = $Env:CUDA_PATH
$CUDAToolkit_NVVM_LIBRARY = "$CUDA\nvvm\lib\x64\nvvm.lib".Replace("\", "/")
$ZLIB_LIBRARY = "$ZLIB\lib\zlib.lib".Replace("\", "/")
$ZLIB_INCLUDE_DIR = "$ZLIB\include".Replace("\", "/")

# Check for some possible mistakes beforehand
If (!(Test-Path -Path "$ZLIB")) {
    throw 'The zlib directory is not valid'
}

If ([string]::IsNullOrEmpty($CUDA) -or !(Test-Path -Path "$CUDA")) {
    Write-Warning 'The CUDA directory is not valid. Proceeding will install without NVidia GPU support'
    $HasCuda = $false
}
Else {
    $HasCuda = $true
}

# Clone or update if necessary
If (!(Test-Path -Path "anydsl")) {
    & $GIT_BIN clone --branch cmake-based-setup https://github.com/AnyDSL/anydsl.git
    cd "anydsl"
}
Else {
    cd "anydsl"
    & $GIT_BIN pull
}

If (!(Test-Path -Path "build")) {
    md "build" > $null
}
cd "build"

$BUILD_TYPE = $Config.AnyDSL_BUILD_TYPE

# Setup cmake
$AnyDSL_Args = @()
$AnyDSL_Args += $Config.CMAKE_EXTRA_ARGS
$AnyDSL_Args += '-DRUNTIME_JIT:BOOL=ON'
$AnyDSL_Args += '-DCMAKE_BUILD_TYPE:STRING="' + $BUILD_TYPE + '"'
$AnyDSL_Args += '-DAnyDSL_runtime_BUILD_SHARED:BOOL=ON'
$AnyDSL_Args += '-DAnyDSL_PKG_LLVM_AUTOBUILD:BOOL=ON'
$AnyDSL_Args += '-DAnyDSL_PKG_LLVM_VERSION:STRING="' + $($Config.AnyDSL_LLVM_VERSION) + '"'
$AnyDSL_Args += '-DAnyDSL_PKG_RV_TAG:STRING="' + $($Config.AnyDSL_RV_TAG) + '"'
$AnyDSL_Args += '-DAnyDSL_PKG_LLVM_URL:STRING="' + $($Config.AnyDSL_LLVM_URL) + '"'
$AnyDSL_Args += '-DLLVM_TARGETS_TO_BUILD:STRING="' + $($Config.AnyDSL_LLVM_TARGETS) + '"'
$AnyDSL_Args += '-DLLVM_ENABLE_PROJECTS:STRING="clang;lld"'
$AnyDSL_Args += '-DLLVM_ENABLE_BINDINGS:BOOL=OFF'
$AnyDSL_Args += '-DTHORIN_PROFILE:BOOL=OFF'
$AnyDSL_Args += '-DZLIB_LIBRARY:PATH="' + $ZLIB_LIBRARY + '"'
$AnyDSL_Args += '-DZLIB_INCLUDE_DIR:PATH="' + $ZLIB_INCLUDE_DIR + '"'

If ($HasCuda) {
    $AnyDSL_Args += '-DCUDAToolkit_NVVM_LIBRARY:PATH="' + $CUDAToolkit_NVVM_LIBRARY + '"'
}

$AnyDSL_Args += $Config.AnyDSL_CMAKE_EXTRA_ARGS

& $CMAKE_BIN $AnyDSL_Args ..

# Build it
& $CMAKE_BIN --build . --config "$BUILD_TYPE" --target pull-thorin pull-artic pull-runtime runtime runtime_jit_artic artic

If (Test-Path -Path "bin\$BUILD_TYPE\") {
    $ANYDSL_BIN_PATH = "bin\$BUILD_TYPE\"
    $LLVM_BIN_PATH = "_deps\llvm-build\$BUILD_TYPE\bin\"
}
Else {
    $ANYDSL_BIN_PATH = "bin\"
    $LLVM_BIN_PATH = "_deps\llvm-build\bin\"
}

# Copy necessary stuff
$ZLIB_DLL = "$ZLIB\bin\zlib.dll"
If (!(Test-Path -Path "$ZLIB_DLL")) {
    Write-Warning 'The zlib dll could not be copied. You might have to copy it yourself'
}
Else {
    cp $ZLIB_DLL "$ANYDSL_BIN_PATH" > $null
    cp $ZLIB_DLL "$LLVM_BIN_PATH" > $null
}

If ($HasCuda) {
    cp "$CUDA\nvvm\bin\nvvm*.dll" "$ANYDSL_BIN_PATH" > $null
}

# Copy to parent dir if necessary
Copy-Item -Exclude "artic.exe" -Path ".\$ANYDSL_BIN_PATH*" -Destination "$BIN_ROOT\" > $null

cd $CURRENT
