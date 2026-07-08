function GetPD {
    param (
        [Parameter(Position = 0)]
        [System.Object]
        $object,
        [Parameter(Position = 1)]
        [System.Object]
        $default
    )
    return ($null -ne $object) ? $object : $default
}

function UseSystemLLVM {
    # True when a usable system/prebuilt LLVM is configured and we are not asked
    # to build from source. Requires <SYSTEM_DIR>/lib/cmake/llvm to exist.
    $sysdir = $Config.LLVM.SYSTEM_DIR
    if ([string]::IsNullOrEmpty($sysdir)) { return $false }
    if (GetPD $Config.LLVM.FORCE_BUILD $false) { return $false }
    return (Test-Path -Path (Join-Path $sysdir "lib/cmake/llvm"))
}

function GetLLVMRoot {
    # Root of the LLVM install to build the AnyDSL stack against: the system one
    # when available, then the prebuilt conda one (Windows default), otherwise
    # the from-source install under deps.
    if (UseSystemLLVM) {
        return $Config.LLVM.SYSTEM_DIR
    }
    if ($IsWindows -and (GetPD $Config.LLVM.PREBUILT $true) -and (Test-Path -Path "$DEPS_ROOT/llvm-conda/Library/lib/cmake/llvm")) {
        return "$DEPS_ROOT/llvm-conda/Library"
    }
    return "$DEPS_ROOT/llvm-install"
}

function HandleGIT {
    param(
        [Parameter(Mandatory)] [string] $Directory,
        [Parameter(Mandatory)] [string] $Branch,
        [Parameter(Mandatory)] [string] $URL,
        [string] $Commit
    )

    If (!(Test-Path -Path $Directory)) {
        if (![string]::IsNullOrEmpty($Commit)) {
            # Pinned commit: fetch exactly that object shallowly.
            & $GIT_BIN init --quiet $Directory
            Set-Location $Directory
            & $GIT_BIN remote add origin $URL
            & $GIT_BIN fetch --depth 1 origin $Commit
            & $GIT_BIN checkout --quiet FETCH_HEAD
        }
        else {
            & $GIT_BIN clone --depth 1 --branch $Branch $URL $Directory
            Set-Location $Directory
        }
    }
    else {
        Set-Location $Directory
        if (![string]::IsNullOrEmpty($Commit)) {
            # Fetch the pinned commit if it is not present yet, then check it out.
            & $GIT_BIN cat-file -e "$Commit^{commit}" 2>$null
            if ($LASTEXITCODE -ne 0) {
                & $GIT_BIN fetch --depth 1 origin $Commit
            }
            & $GIT_BIN checkout --quiet $Commit
        }
        else {
            & $GIT_BIN pull origin

            # Check if it is a named ref and can be pulled
            $ref = (& $GIT_BIN show-ref refs/remotes/origin/$Branch)
            if ([string]::IsNullOrEmpty($ref)) {
                $is_shallow = (& $GIT_BIN rev-parse --is-shallow-repository)
                if ($is_shallow -eq "true") {
                    & $GIT_BIN fetch --unshallow --quiet
                }
                else {
                    & $GIT_BIN fetch --quiet
                }
            }
            & $GIT_BIN checkout --quiet $Branch
        }
    }
}

function RenameDLL {
    param(
        [Parameter(Mandatory)] [string] $InputDLL,
        [Parameter(Mandatory)] [string] $OutputDLL
    )

    # The DLL can be copied as it is
    Copy-Item -Path $InputDLL -Destination $OutputDLL

    # Copy the lib file takes some work
    $OutputDef = [System.IO.Path]::ChangeExtension($OutputDLL, ".def")
    $OutputLib = [System.IO.Path]::ChangeExtension($OutputDLL, ".lib")

    $dumpbin = (dumpbin.exe /Exports $InputDLL)
    
    $lib_file = "EXPORTS `n"
    foreach ($line in $dumpbin) {
        $match = [regex]::Matches($line, "^\s*(\d+)\s+[A-Z0-9]+\s+[A-Z0-9]{8}\s+([^ ]+)")
        if ($match) {
            $func = $match.Groups[2].Value
            if ($null -ne $func) {
                $lib_file = $lib_file + $func + "`n"
            }
        }
    }

    $lib_file | Out-File $OutputDef

    & lib.exe /MACHINE:X64 /DEF:$OutputDef /OUT:$OutputLib /NOLOGO
}