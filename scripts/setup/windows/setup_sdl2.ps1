$CURRENT=Get-Location

# Change the following lines to fit your own system:
# CHANGE AFTER THIS ----------------------------------
$SDL2_URL="https://github.com/libsdl-org/SDL/releases/download/release-2.24.0/SDL2-devel-2.24.0-VC.zip"
# NO CHANGE AFTER THIS -------------------------------

cd "tmp"

# Get zip
if (!(Test-Path "sdl2.zip")) {
    Invoke-WebRequest -Uri "$SDL2_URL" -OutFile "sdl2.zip"
    Expand-Archive sdl2.zip -DestinationPath .
}

# Copy
$SDL2_ROOT="$DEPS_ROOT/SDL2"
If (!(test-path "$SDL2_ROOT")) {
    md "$SDL2_ROOT"
    Copy-Item -Recurse -Path "SDL2*/*" -Destination "$SDL2_ROOT"
}

cp "$SDL2_ROOT\lib\x64\SDL2.dll" "$BIN_ROOT\"

cd $CURRENT
