$CURRENT=Get-Location

cd "tmp"

# Get zip
if (!(Test-Path "zlib.zip")) {
    Invoke-WebRequest -Uri "$($Config.ZLIB_URL)" -OutFile "zlib.zip"
    Expand-Archive zlib.zip -DestinationPath . > $null
}
cd "zlib*/"

# Configure
$ZLIB_ROOT="$DEPS_ROOT\zlib".Replace("\", "/")
& $CMAKE_BIN $Config.CMAKE_EXTRA_ARGS -DCMAKE_BUILD_TYPE="$($Config.ZLIB_BUILD_TYPE)" -DCMAKE_INSTALL_PREFIX="$ZLIB_ROOT" .

# Build it
& $CMAKE_BIN --build . --config "$($Config.ZLIB_BUILD_TYPE)"

# Install it
& $CMAKE_BIN --install .

cd $CURRENT
