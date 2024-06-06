$CURRENT = Get-Location

if ($IsLinux) {
    Write-Output "Using system libraries for SDL2"
    return
}

Set-Location "tmp"

# Get zip
if (!(Test-Path "sdl2.zip")) {
    Invoke-WebRequest -UserAgent "Wget" -Uri $Config.SDL2.URL -OutFile "sdl2.zip"
    Expand-Archive sdl2.zip -DestinationPath . > $null
}

# Copy
$SDL2_ROOT = "$DEPS_ROOT/SDL2"
If (!(Test-Path "$SDL2_ROOT")) {
    mkdir "$SDL2_ROOT" > $null
    Copy-Item -Recurse -Path "SDL2*/*" -Destination "$SDL2_ROOT" > $null
}

Copy-Item -Path "$SDL2_ROOT\lib\x64\SDL2.dll" -Destination "$BIN_ROOT\" > $null

Set-Location $CURRENT
