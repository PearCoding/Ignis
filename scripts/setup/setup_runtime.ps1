$CURRENT = Get-Location

# Some predefined locations
$CUDA = $(Get-ChildItem env: | Where-Object { $_.Name -like "CUDA_PATH*" })[0].Value
$CUDAToolkit_NVVM_LIBRARY = "$CUDA\nvvm\lib\x64\nvvm.lib".Replace("\", "/").Replace(" ", "` ")
$ARTIC = "$DEPS_ROOT\artic".Replace("\", "/").Replace(" ", "` ")
$LLVM = "$DEPS_ROOT\llvm-install".Replace("\", "/").Replace(" ", "` ")
$TBB="$DEPS_ROOT\tbb".Replace("\", "/")

# Check for some possible mistakes beforehand

If (!(Test-Path -Path "$ARTIC")) {
    throw 'The artic directory is not valid'
}

If (!(Test-Path -Path "$LLVM")) {
    throw 'The LLVM directory is not valid'
}

If (!(Test-Path -Path "$TBB")) {
    throw 'The TBB directory is not valid'
}

$BUILD_TYPE = $Config.RUNTIME.BUILD_TYPE

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
function CompileRuntime {
    param (
        [ValidateNotNullOrEmpty()]
        [string]$Device = "default"
    )

    if ($Device -eq 'default') {
        $runtime_dir = "runtime"
    } else {
        $runtime_dir = "runtime-$Device"
    }

    # Clone or update if necessary
    If (!(Test-Path -Path $runtime_dir)) {
        & $GIT_BIN clone --depth 1 --branch $Config.RUNTIME.BRANCH $Config.RUNTIME.GIT $runtime_dir
        Set-Location $runtime_dir
    } else {
        Set-Location $runtime_dir
        & $GIT_BIN pull
    }
    
    # Setup cmake
    $CMAKE_Args = @()
    $CMAKE_Args += $Config.CMAKE.EXTRA_ARGS
    $CMAKE_Args += $Config.RUNTIME.EXTRA_ARGS
    $CMAKE_Args += '-DCMAKE_BUILD_TYPE:STRING=' + $BUILD_TYPE
    # $CMAKE_Args += '-DCMAKE_INSTALL_PREFIX:PATH=install' 
    $CMAKE_Args += '-DArtic_DIR:PATH=' + "$ARTIC/build/share/anydsl/cmake"
    $CMAKE_Args += '-DLLVM_DIR:PATH=' + "$LLVM/lib/cmake/llvm"
    $CMAKE_Args += '-DTBB_DIR:PATH=' + "$TBB/lib/cmake/tbb"
    $CMAKE_Args += '-DBUILD_SHARED_LIBS:BOOL=ON'
    $CMAKE_Args += '-DRUNTIME_JIT:BOOL=ON'
    $CMAKE_Args += '-DCMAKE_REQUIRE_FIND_PACKAGE_LLVM:BOOL=ON' # We really want JIT
    $CMAKE_Args += '-DCMAKE_DISABLE_FIND_PACKAGE_OpenCL:BOOL=ON' # Not supported

    if (($Device -eq 'default') || ($Device -eq 'cuda')) {
        If ($HasCuda) {
            $CMAKE_Args += '-DCUDAToolkit_NVVM_LIBRARY:PATH=' + $CUDAToolkit_NVVM_LIBRARY
        } else {
            Write-Warning 'No CUDA support. Proceeding will not build the runtime with Nvidia GPU support'
            if ($Device -ne 'default') {
                return
            }
        }
    } else {
        # $CMAKE_Args += '-DCMAKE_DISABLE_FIND_PACKAGE_CUDA:BOOL=ON'
        $CMAKE_Args += '-DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit:BOOL=ON'
    }

    if (($Device -eq "amd_hsa") || ($Device -eq "amd_pal")) {
        Write-Warning 'Support for AMD GPUs over this setup is not supported yet. Sorry :/'
        return
    } elseif ($Device -ne 'default') {
        $CMAKE_Args += '-DCMAKE_DISABLE_FIND_PACKAGE_hsa-runtime64:BOOL=ON'
        $CMAKE_Args += '-DCMAKE_DISABLE_FIND_PACKAGE_pal:BOOL=ON'
    }

    & $CMAKE_BIN -S . -B build $CMAKE_Args 

    if ($LASTEXITCODE -ne 0) {
        throw "Failed to configure AnyDSL runtime for $Device"
    }

    # Build it
    & $CMAKE_BIN --build build --config "$BUILD_TYPE" --parallel $Config.CMAKE.PARALLEL_JOBS

    if ($LASTEXITCODE -ne 0) {
        throw "Failed to build AnyDSL runtime for $Device"
    }

    # & $CMAKE_BIN --install build --config "$BUILD_TYPE"

    # if ($LASTEXITCODE -ne 0) {
    #     throw "Failed to install LLVM"
    # }

    # Rename stuff if necessary
    if ($Device -ne 'default') {
        RenameDLL -InputDLL 'build/bin/runtime.dll' -OutputDLL "build/bin/runtime_$device.dll"
        RenameDLL -InputDLL 'build/bin/runtime_jit_artic.dll' -OutputDLL "build/bin/runtime_jit_artic_$device.dll"
        # Copy-Item -Path 'build/bin/runtime.dll' -Destination "build/bin/runtime_$device.dll"
        # Copy-Item -Path 'build/bin/runtime_jit_artic.dll' -Destination "build/bin/runtime_jit_artic_$device.dll"
        # Copy-Item -Path 'build/lib/runtime.lib' -Destination "build/lib/runtime_$device.lib"
        # Copy-Item -Path 'build/lib/runtime_jit_artic.lib' -Destination "build/lib/runtime_jit_artic_$device.lib"

        Copy-Item -Path "build/bin/runtime_$device.dll" -Destination "$BIN_ROOT\" > $null
        Copy-Item -Path "build/bin/runtime_jit_artic_$device.dll" -Destination "$BIN_ROOT\" > $null
    } else {
        Copy-Item -Path "build/bin/*" -Destination "$BIN_ROOT\" > $null
    }

    Set-Location $CURRENT
}

# Compile basic version
CompileRuntime

# Compile per variant stuff
foreach ($device in $Config.RUNTIME.DEVICES) {
    CompileRuntime -Device $device
    Set-Location $CURRENT
}
Set-Location $CURRENT