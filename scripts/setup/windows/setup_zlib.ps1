$CURRENT=Get-Location

cd "tmp"

# Get zip
if (!(Test-Path "zlib.zip")) {
    Invoke-WebRequest -UserAgent "Wget" -Uri "$($Config.ZLIB_URL)" -OutFile "zlib.zip"
    Expand-Archive zlib.zip -DestinationPath . > $null
}
cd "zlib*/"

# Configure
$ZLIB_ROOT="$DEPS_ROOT\zlib".Replace("\", "/")
& $CMAKE_BIN $Config.CMAKE_EXTRA_ARGS -DCMAKE_BUILD_TYPE="$($Config.ZLIB_BUILD_TYPE)" -DCMAKE_INSTALL_PREFIX="$ZLIB_ROOT" .

if ($LASTEXITCODE -ne 0) {
    throw "Failed to configure zlib"
}

# Build it
& $CMAKE_BIN --build . --config "$($Config.ZLIB_BUILD_TYPE)"

if ($LASTEXITCODE -ne 0) {
    throw "Failed to build zlib"
}

# Install it
& $CMAKE_BIN --install .

if ($LASTEXITCODE -ne 0) {
    throw "Failed to install zlib"
}

cd $CURRENT
