$CURRENT=Get-Location

cd "tmp"

# Get zip
if (!(Test-Path "half.zip")) {
    Invoke-WebRequest -UserAgent "Wget" -Uri "$($Config.HALF_URL)" -OutFile "half.zip"
    Expand-Archive half.zip -DestinationPath half > $null
}

# Copy
$HALF_ROOT="$DEPS_ROOT/half"
If (!(Test-Path "$HALF_ROOT")) {
    md "$HALF_ROOT" > $null
    Copy-Item -Recurse -Path "half/include" -Destination "$HALF_ROOT\include" > $null
}

cd $CURRENT
