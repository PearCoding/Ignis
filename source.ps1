
# ***************************************************************
# * This script adds Ignis to the current path on Windows.
# * It assumes that Ignis is either compiled within a 
# * a subdirectory named 'build' or within a subdirectory named 'build/Release'.
# ***************************************************************

$env:IGNIS_DIR=$PSScriptRoot

$BUILD_DIR="$env:IGNIS_DIR\build"
$BIN_DIR="$BUILD_DIR\bin\Release"
$LIB_DIR="$BUILD_DIR\lib\Release"
$API_DIR="$BUILD_DIR\api"

If (!(Test-Path $BIN_DIR)) {
    $BIN_DIR="$BUILD_DIR\Release\bin"
    $LIB_DIR="$BUILD_DIR\Release\lib"
    $API_DIR="$BUILD_DIR\Release\api"

    If (!(Test-Path $BIN_DIR)) {
        $BIN_DIR="$BUILD_DIR\bin"
        $LIB_DIR="$BUILD_DIR\lib"
        $API_DIR="$BUILD_DIR\api"
    }
}

$env:PATH=$env:PATH + ";$BIN_DIR;$LIB_DIR"
$env:PYTHONPATH=$env:PYTHONPATH + ";$API_DIR"
