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

function RenameDLL {
    param(
        [Parameter(Mandatory)] [string] $InputDLL,
        [Parameter(Mandatory)] [string] $OutputDLL
    )

    # The DLL can be copied as it is
    Copy-Item -Path $InputDLL -Destination $OutputDLL

    # Copy the lib file takes some work
    $OutputDef = [System.IO.Path]::ChangeExtension($OutputDLL,".def")
    $OutputLib = [System.IO.Path]::ChangeExtension($OutputDLL,".lib")

    $dumpbin = (dumpbin.exe /Exports $InputDLL)
    
    $lib_file = "EXPORTS `n"
    foreach($line in $dumpbin) {
        $match = [regex]::Matches($line, "^\s*(\d+)\s+[A-Z0-9]+\s+[A-Z0-9]{8}\s+([^ ]+)")
        if ($match) {
            $func = $match.Groups[2].Value
            if($null -ne $func) {
                $lib_file = $lib_file + $func + "`n"
            }
        }
    }

    $lib_file | Out-File $OutputDef

    & lib.exe /MACHINE:X64 /DEF:$OutputDef /OUT:$OutputLib /NOLOGO
}