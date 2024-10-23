$CURRENT_DIR = Get-Location

$SCRIPT = "$PSScriptRoot/../../../scripts/RunRIF.ps1"
function HandleScene {
    param (
        [string]$Filename
    )

    & $SCRIPT -Output "ref-$Filename-rad" "$Filename.rif"
    Write-Host "-----------------------------" -ForegroundColor Magenta
}

Set-Location $PSScriptRoot

try {
    # HandleScene "plane-array-diffuse"
    # # Move-Item -Force "ref-plane-array-diffuse.exr" "ref-plane-array-diffuse-rad.exr"
    # HandleScene "plane-array-klems"
    # Move-Item -Force "ref-plane-array-klems-rad_0.exr" "ref-plane-array-klems-front-rad.exr"
    # Move-Item -Force "ref-plane-array-klems-rad_1.exr" "ref-plane-array-klems-back-rad.exr"
    # HandleScene "plane-array-tensortree"
    # Move-Item -Force "ref-plane-array-tensortree-rad_0.exr" "ref-plane-array-tensortree-front-rad.exr"
    # Move-Item -Force "ref-plane-array-tensortree-rad_1.exr" "ref-plane-array-tensortree-back-rad.exr"
    # HandleScene "plane-array-tensortree-t3"
    # Move-Item -Force "ref-plane-array-tensortree-t3-rad_0.exr" "ref-plane-array-tensortree-t3-front-rad.exr"
    # Move-Item -Force "ref-plane-array-tensortree-t3-rad_1.exr" "ref-plane-array-tensortree-t3-back-rad.exr"
    # HandleScene "sky-uniform"
    # HandleScene "sky-clear"
    # HandleScene "sky-cloudy"
    # HandleScene "sky-intermediate"
    # HandleScene "sky-perez1"
    # HandleScene "sky-perez2"
    # HandleScene "sky-perez3"
    HandleScene "sun-on-plane"
}
catch {
    Write-Host "Update failed:" -ForegroundColor Red
    Write-Host $_ -ForegroundColor Red
}

Set-Location $CURRENT_DIR