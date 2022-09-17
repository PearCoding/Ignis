$CURRENT=Get-Location

cd "tmp"

# Get zip
if (!(Test-Path "zlib.zip")) {
    Invoke-WebRequest -Uri "$($Config.ZLIB_URL)" -OutFile "zlib.zip"
    Expand-Archive zlib.zip -DestinationPath .
}
cd "zlib*/"

# Configure
$ZLIB_ROOT="$DEPS_ROOT\zlib".Replace("\", "/")
cmake -DCMAKE_BUILD_TYPE="$($Config.ZLIB_BUILD_TYPE)" -DCMAKE_INSTALL_PREFIX="$ZLIB_ROOT" .

# Build it
cmake --build . --config "$($Config.ZLIB_BUILD_TYPE)"

# Install it
cmake --install .

cd $CURRENT
