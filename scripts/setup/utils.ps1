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

function HandleGIT {
    param(
        [Parameter(Mandatory)] [string] $Directory,
        [Parameter(Mandatory)] [string] $Branch,
        [Parameter(Mandatory)] [string] $URL
    )

    If (!(Test-Path -Path $Directory)) {
        & $GIT_BIN clone --depth 1 --branch $Branch $URL $Directory
        Set-Location $Directory
    }
    else {
        Set-Location $Directory
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