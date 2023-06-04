$CURRENT=Get-Location

cd "tmp"

# Get zip
if (!(Test-Path "tbb.zip")) {
    Invoke-WebRequest -Uri "$($Config.TBB_URL)" -OutFile "tbb.zip"
    Expand-Archive tbb.zip -DestinationPath . > $null
}

# Copy
$TBB_ROOT="$DEPS_ROOT/tbb"
If (!(Test-Path "$TBB_ROOT")) {
    md "$TBB_ROOT" > $null
    Copy-Item -Recurse -Path "oneapi*\include" -Destination "$TBB_ROOT\include" > $null
    Copy-Item -Recurse -Path "oneapi*\lib" -Destination "$TBB_ROOT\lib" > $null
}

If ($Config.IGNIS_BUILD_TYPE == "Release") {
    cp "oneapi*\redist\intel64\vc14\tbb12.dll" "$BIN_ROOT\" > $null
    cp "oneapi*\redist\intel64\vc14\tbbmalloc.dll" "$BIN_ROOT\" > $null
} else {
    cp "oneapi*\redist\intel64\vc14\tbb12_debug.dll" "$BIN_ROOT\" > $null
    cp "oneapi*\redist\intel64\vc14\tbbmalloc_debug.dll" "$BIN_ROOT\" > $null
}

cd $CURRENT
