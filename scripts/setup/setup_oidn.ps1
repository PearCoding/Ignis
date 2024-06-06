$CURRENT = Get-Location

# TODO: This might be a good idea to support
if ($IsLinux) {
    Write-Output "Using system libraries for OIDN"
    return
}

Set-Location "tmp"

# Get zip
if (!(Test-Path "oidn.zip")) {
    Invoke-WebRequest -UserAgent "Wget" -Uri $Config.OIDN.URL -OutFile "oidn.zip"
    Expand-Archive oidn.zip -DestinationPath . > $null
}

# Copy
$OIDN_ROOT = "$DEPS_ROOT/oidn"
If (!(Test-Path "$OIDN_ROOT")) {
    mkdir "$OIDN_ROOT" > $null
}

$EX_DIR = Get-ChildItem -Path "oidn*" -Directory

robocopy "$($EX_DIR.FullName)\include" "$OIDN_ROOT\include" /mir  > $null
robocopy "$($EX_DIR.FullName)\lib" "$OIDN_ROOT\lib" /mir > $null
robocopy "$($EX_DIR.FullName)\bin" "$OIDN_ROOT\bin" /mir > $null
robocopy "$($EX_DIR.FullName)\bin" "$BIN_ROOT\" "OpenImageDenoise*.dll" /mir > $null

Set-Location $CURRENT
