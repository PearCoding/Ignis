
# ***************************************************************
# * This script adds Ignis to the current path on Windows.
# * It assumes that Ignis is either compiled within a 
# * a subdirectory named 'build' or within a subdirectory named 'build/Release'.
# ***************************************************************

$env:IGNIS_DIR=Get-Location
$env:PATH=$env:PATH + ";" + $env:IGNIS_DIR + "\build\bin;" + $env:IGNIS_DIR + "\build\Release\bin"
$env:PATH=$env:PATH + ";" + $env:IGNIS_DIR + "\build\lib;" + $env:IGNIS_DIR + "\build\Release\lib"
$env:PYTHONPATH=$env:PYTHONPATH + ";" +$env:IGNIS_DIR + "\build\api;" + $env:IGNIS_DIR + "\build\Release\api"
