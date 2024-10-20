$CURRENT = Get-Location

# Some predefined locations
$ARTIC = "$DEPS_ROOT\artic".Replace("\", "/").Replace(" ", "` ")
$LLVM = "$DEPS_ROOT\llvm-install".Replace("\", "/").Replace(" ", "` ")
$TBB = "$DEPS_ROOT\tbb".Replace("\", "/")

# Check for some possible mistakes beforehand

If (!(Test-Path -Path "$ARTIC")) {
    throw 'The artic directory is not valid'
}

If (!(Test-Path -Path "$LLVM")) {
    throw 'The LLVM directory is not valid'
}

If (!$IsLinux -and !(Test-Path -Path "$TBB")) {
    throw 'The TBB directory is not valid'
}

$BUILD_TYPE = $Config.RUNTIME.BUILD_TYPE

If ($IsLinux) {
    $HasCuda = $true
} else {
    $CUDA = $(Get-ChildItem env: | Where-Object { $_.Name -like "CUDA_PATH*" })[0].Value
    $CUDAToolkit_NVVM_LIBRARY = "$CUDA\nvvm\lib\x64\nvvm.lib".Replace("\", "/").Replace(" ", "` ")
    If ([string]::IsNullOrEmpty($CUDA) -or !(Test-Path -Path "$CUDA")) {
        Write-Warning 'The CUDA directory is not valid. Proceeding will install without Nvidia GPU support'
        $HasCuda = $false
    }
    Else {
        $HasCuda = $true
    }

    If ($HasCuda) {
        Copy-Item "$CUDA\nvvm\bin\nvvm*.dll" "$BIN_ROOT/" > $null
    }
}

# Clone or update if necessary
HandleGIT runtime $Config.RUNTIME.BRANCH $Config.RUNTIME.GIT

function CompileRuntime {
    param (
        [ValidateNotNullOrEmpty()]
        [string]$Device = "default"
    )

    if ($Device -eq 'default') {
        $runtime_name = "runtime"
        $build_dir = "build"
    }
    else {
        $runtime_name = "runtime_$Device"
        $build_dir = "build_$Device"
    }

    # Setup cmake
    $CMAKE_Args = @()
    $CMAKE_Args += $Config.CMAKE.EXTRA_ARGS
    $CMAKE_Args += $Config.RUNTIME.EXTRA_ARGS
    $CMAKE_Args += '-DCMAKE_BUILD_TYPE:STRING=' + $BUILD_TYPE
    # $CMAKE_Args += '-DCMAKE_INSTALL_PREFIX:PATH=install' 
    $CMAKE_Args += '-DArtic_DIR:PATH=' + "$ARTIC/build/share/anydsl/cmake"
    $CMAKE_Args += '-DLLVM_DIR:PATH=' + "$LLVM/lib/cmake/llvm"
    If (!$IsLinux) {
        $CMAKE_Args += '-DTBB_DIR:PATH=' + "$TBB/lib/cmake/tbb"
    }
    $CMAKE_Args += '-DBUILD_SHARED_LIBS:BOOL=ON'
    $CMAKE_Args += '-DRUNTIME_JIT:BOOL=ON'
    $CMAKE_Args += '-DCMAKE_REQUIRE_FIND_PACKAGE_LLVM:BOOL=ON' # We really want JIT
    $CMAKE_Args += '-DCMAKE_DISABLE_FIND_PACKAGE_OpenCL:BOOL=ON' # Not supported
    $CMAKE_Args += '-DAnyDSL_runtime_TARGET_NAME=' + $runtime_name

    if (($Device -eq 'default') -or ($Device -eq 'cuda')) {
        if (!$IsLinux){
            If ($HasCuda) {
                $CMAKE_Args += '-DCUDAToolkit_NVVM_LIBRARY:PATH=' + $CUDAToolkit_NVVM_LIBRARY
            }
            else {
                Write-Warning 'No CUDA support. Proceeding will not build the runtime with Nvidia GPU support'
                if ($Device -ne 'default') {
                    return
                }
            }
        }
    }
    else {
        # $CMAKE_Args += '-DCMAKE_DISABLE_FIND_PACKAGE_CUDA:BOOL=ON'
        $CMAKE_Args += '-DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit:BOOL=ON'
    }

    if (($Device -eq "amd_hsa") -or ($Device -eq "amd_pal")) {
        Write-Warning 'Support for AMD GPUs over this setup is not supported yet. Sorry :/'
        return
    }
    elseif ($Device -ne 'default') {
        $CMAKE_Args += '-DCMAKE_DISABLE_FIND_PACKAGE_hsa-runtime64:BOOL=ON'
        $CMAKE_Args += '-DCMAKE_DISABLE_FIND_PACKAGE_pal:BOOL=ON'
    }

    & $CMAKE_BIN -S . -B $build_dir $CMAKE_Args 

    if ($LASTEXITCODE -ne 0) {
        throw "Failed to configure AnyDSL runtime for $Device"
    }

    # Build it
    & $CMAKE_BIN --build $build_dir --config "$BUILD_TYPE" --parallel $Config.CMAKE.PARALLEL_JOBS

    if ($LASTEXITCODE -ne 0) {
        throw "Failed to build AnyDSL runtime for $Device"
    }

    # & $CMAKE_BIN --install $build_dir --config "$BUILD_TYPE"

    # if ($LASTEXITCODE -ne 0) {
    #     throw "Failed to install LLVM"
    # }

    # Copy dlls
    if ($IsWindows) {
        Copy-Item -Path "$build_dir/bin/*" -Destination "$BIN_ROOT\" > $null
    }
}

# Compile basic version
CompileRuntime

# Compile per variant stuff
foreach ($device in $Config.RUNTIME.DEVICES) {
    CompileRuntime -Device $device
}

Set-Location $CURRENT