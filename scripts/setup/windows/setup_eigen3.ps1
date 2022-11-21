$CURRENT=Get-Location

cd "tmp"

# Get zip
if (!(Test-Path "eigen3.zip")) {
    Invoke-WebRequest -Uri "$($Config.Eigen3_URL)" -OutFile "eigen3.zip"
    Expand-Archive eigen3.zip -DestinationPath .
}
cd "eigen*/"

If (!(test-path "build")) {
    md "build"
}
cd "build"

# Configure it
$EIGEN3_ROOT="$DEPS_ROOT\Eigen3".Replace("\", "/")
& $CMAKE_BIN $Config.CMAKE_EXTRA_ARGS -DCMAKE_BUILD_TYPE="Release" -DCMAKE_INSTALL_PREFIX="$EIGEN3_ROOT" -DCMAKE_INSTALL_MESSAGE="LAZY" ..

# Build it
& $CMAKE_BIN --build . --config "Release"

# Install it
& $CMAKE_BIN --install .

cd $CURRENT
