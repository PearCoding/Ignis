$CURRENT=Get-Location

cd "tmp"

# Get zip
if (!(Test-Path "tbb.zip")) {
    Invoke-WebRequest -Uri "$($Config.TBB_URL)" -OutFile "tbb.zip"
    Expand-Archive tbb.zip -DestinationPath .
}

# Copy
$TBB_ROOT="$DEPS_ROOT/tbb"
If (!(test-path "$TBB_ROOT")) {
    md "$TBB_ROOT"
    Copy-Item -Recurse -Path "oneapi*\include" -Destination "$TBB_ROOT\include"
    Copy-Item -Recurse -Path "oneapi*\lib" -Destination "$TBB_ROOT\lib"
}

cp "oneapi*\redist\intel64\vc14\tbb12.dll" "$BIN_ROOT\"
cp "oneapi*\redist\intel64\vc14\tbbmalloc.dll" "$BIN_ROOT\"

cd $CURRENT
