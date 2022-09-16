$CURRENT=Get-Location

# Change the following lines to fit your own system:
# CHANGE AFTER THIS ----------------------------------
$EIGEN3_URL="https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.zip"
# NO CHANGE AFTER THIS -------------------------------

cd "tmp"

# Get zip
if (!(Test-Path "eigen3.zip")) {
    Invoke-WebRequest -Uri "$EIGEN3_URL" -OutFile "eigen3.zip"
    Expand-Archive eigen3.zip -DestinationPath .
}
cd "eigen*/"

If (!(test-path "build")) {
    md "build"
}
cd "build"

# Configure it
$EIGEN3_ROOT="$DEPS_ROOT\Eigen3".Replace("\", "/")
cmake -DCMAKE_BUILD_TYPE="Release" -DCMAKE_INSTALL_PREFIX="$EIGEN3_ROOT" ..

# Build it
cmake --build . --config "Release"

# Install it
cmake --install .

cd $CURRENT
