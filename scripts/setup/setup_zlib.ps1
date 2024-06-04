$CURRENT = Get-Location

if($IsLinux) {
    Write-Output "Using system libraries for zlib"
    return
}

Set-Location "tmp"

# Get zip
if (!(Test-Path "zlib*/")) {
    if ([IO.Path]::GetExtension($Config.ZLIB.URL) -eq ".zip") {
        Invoke-WebRequest -UserAgent "Wget" -Uri $Config.ZLIB.URL -OutFile "zlib.zip"
        Expand-Archive zlib.zip -DestinationPath . > $null
    }
    else {
        Invoke-WebRequest -UserAgent "Wget" -Uri $Config.ZLIB.URL -OutFile "zlib.tar.gz"
        tar -xzf zlib.tar.gz -C ./
    }
}
Set-Location "zlib*/"

# Configure
$ZLIB_ROOT = "$DEPS_ROOT\zlib".Replace("\", "/")
& $CMAKE_BIN $Config.CMAKE.EXTRA_ARGS -DCMAKE_BUILD_TYPE="$($Config.ZLIB.BUILD_TYPE)" -DCMAKE_INSTALL_PREFIX="$ZLIB_ROOT" .

if ($LASTEXITCODE -ne 0) {
    throw "Failed to configure zlib"
}

# Build it
& $CMAKE_BIN --build . --config "$($Config.ZLIB.BUILD_TYPE)" --parallel $Config.CMAKE.PARALLEL_JOBS

if ($LASTEXITCODE -ne 0) {
    throw "Failed to build zlib"
}

# Install it
& $CMAKE_BIN --install . --config "$($Config.ZLIB.BUILD_TYPE)"

if ($LASTEXITCODE -ne 0) {
    throw "Failed to install zlib"
}

# Install binaries
Copy-Item -Path "$ZLIB_ROOT/bin/*" -Destination "$BIN_ROOT/" > $null

Set-Location $CURRENT
