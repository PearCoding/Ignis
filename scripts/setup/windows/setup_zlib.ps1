$CURRENT=Get-Location

# Change the following lines to fit your own system:
# CHANGE AFTER THIS ----------------------------------
$ZLIB_URL="https://zlib.net/zlib1212.zip"
# NO CHANGE AFTER THIS -------------------------------

cd "tmp"

# Get zip
if (!(Test-Path "zlib.zip")) {
    Invoke-WebRequest -Uri "$ZLIB_URL" -OutFile "zlib.zip"
    Expand-Archive zlib.zip -DestinationPath .
}
cd "zlib*/"

# Configure
$ZLIB_ROOT="$DEPS_ROOT\zlib".Replace("\", "/")
cmake -DCMAKE_BUILD_TYPE="Release" -DCMAKE_INSTALL_PREFIX="$ZLIB_ROOT" .

# Build it
cmake --build . --config "Release"

# Install it
cmake --install .

cd $CURRENT
