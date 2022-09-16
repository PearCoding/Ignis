$CURRENT=Get-Location

# Get VC location
$VC_ROOT="C:\Program Files\Microsoft Visual Studio\2022"
If (!(test-path $VC_ROOT)) {
    $VC_ROOT="C:\Program Files (x86)\Microsoft Visual Studio\2019"
}

$VC="$VC_ROOT\Community\"
If (!(test-path $VC)) {
    $VC="$VC_ROOT\Professional\"
    If (!(test-path $VC)) {
        $VC="$VC_ROOT\Enterprise\"
    }
}

# Check for some possible mistakes beforehand
If (!(test-path $VC)) {
    throw 'The VC directory is not valid'
}

# Setup dev environment
& $VC\Common7\Tools\Launch-VsDevShell.ps1

cd $CURRENT
