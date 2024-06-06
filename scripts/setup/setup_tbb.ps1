$CURRENT = Get-Location

if ($IsLinux) {
    Write-Output "Using system libraries for tbb"
    return
}

Set-Location "tmp"

# Get zip
if (!(Test-Path "tbb.zip")) {
    Invoke-WebRequest -UserAgent "Wget" -Uri "$($Config.TBB.URL)" -OutFile "tbb.zip"
    Expand-Archive tbb.zip -DestinationPath . > $null
}

# Copy
$TBB_ROOT = "$DEPS_ROOT/tbb"
If (!(Test-Path "$TBB_ROOT")) {
    mkdir "$TBB_ROOT" > $null
    Copy-Item -Recurse -Path "oneapi*\include" -Destination "$TBB_ROOT\include" > $null
    Copy-Item -Recurse -Path "oneapi*\lib" -Destination "$TBB_ROOT\lib" > $null
    Copy-Item -Recurse -Path "oneapi*\redist" -Destination "$TBB_ROOT\redist" > $null
}

Copy-Item -Path "oneapi*\redist\intel64\vc14\tbb12.dll" -Destination "$BIN_ROOT\" > $null
Copy-Item -Path "oneapi*\redist\intel64\vc14\tbbmalloc.dll" -Destination "$BIN_ROOT\" > $null
Copy-Item -Path "oneapi*\redist\intel64\vc14\tbb12_debug.dll" -Destination "$BIN_ROOT\" > $null
Copy-Item -Path "oneapi*\redist\intel64\vc14\tbbmalloc_debug.dll" -Destination "$BIN_ROOT\" > $null

Set-Location $CURRENT
