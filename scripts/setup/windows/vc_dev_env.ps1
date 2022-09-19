$CURRENT=Get-Location

# Get VC location
$VsWherePath = "`"${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe`""
$VsData = Invoke-Expression "& $VsWherePath $whereArgs -format json" | ConvertFrom-Json

$VC=$VsData.installationPath
If (!(test-path $VC)) {
    throw 'The VC directory is not valid'
}

# Setup dev environment
& $VC\Common7\Tools\Launch-VsDevShell.ps1 -Arch amd64

cd $CURRENT
