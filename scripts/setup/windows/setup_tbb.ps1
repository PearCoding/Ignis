$CURRENT=Get-Location

# Change the following lines to fit your own system:
# CHANGE AFTER THIS ----------------------------------
$TBB_URL="https://github.com/oneapi-src/oneTBB/releases/download/v2021.5.0/oneapi-tbb-2021.5.0-win.zip"
# NO CHANGE AFTER THIS -------------------------------

cd "tmp"

# Get zip
if (!(Test-Path "tbb.zip")) {
    Invoke-WebRequest -Uri "$TBB_URL" -OutFile "tbb.zip"
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
