$CURRENT=Get-Location

cd "tmp"

# Get zip
if (!(Test-Path "oidn.zip")) {
    Invoke-WebRequest -Uri "$($Config.OIDN_URL)" -OutFile "oidn.zip"
    Expand-Archive oidn.zip -DestinationPath . > $null
}

# Copy
$OIDN_ROOT="$DEPS_ROOT/oidn"
If (!(Test-Path "$OIDN_ROOT")) {
    md "$OIDN_ROOT" > $null
}

$EX_DIR=Get-ChildItem -Path "oidn*" -Directory

robocopy "$($EX_DIR.FullName)\include" "$OIDN_ROOT\include" /mir  > $null
robocopy "$($EX_DIR.FullName)\lib" "$OIDN_ROOT\lib" /mir > $null
robocopy "$($EX_DIR.FullName)\bin" "$OIDN_ROOT\bin" /mir > $null
robocopy "$($EX_DIR.FullName)\bin" "$BIN_ROOT\" "OpenImageDenoise*.dll" /mir > $null

cd $CURRENT
