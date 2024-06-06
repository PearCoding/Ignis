$CURRENT=Get-Location

if($IsLinux) {
    Write-Output "Using system libraries for half"
    return
}

cd "tmp"

# Get zip
if (!(Test-Path "half.zip")) {
    Invoke-WebRequest -UserAgent "Wget" -Uri $Config.HALF.URL -OutFile "half.zip"
    Expand-Archive half.zip -DestinationPath half > $null
}

# Copy
$HALF_ROOT="$DEPS_ROOT/half"
If (!(Test-Path "$HALF_ROOT")) {
    md "$HALF_ROOT" > $null
    Copy-Item -Recurse -Path "half/include" -Destination "$HALF_ROOT\include" > $null
}

cd $CURRENT