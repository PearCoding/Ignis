# Basic script running Radiance in a "Ignis"-compatible way and generating an EXR (instead of a HDR)
# Use `run_rif.ps1 RIF_FILE`
# In contrary to rad *.rif, this one does not cache files nor does is share the same command line parameters!
# In contrary to run_rad.ps1, this one extracts most information in the given rif file.
# The script ignores QUALITY, DETAIL, VARIABILITY and EXPOSURE however.
# The intended use case is limited to short renderings for comparison purposes
# `oconv`, `rpict`, `rtrace` and `vwrays` (all Radiance tools) have to be available in the current scope

param (
    [string]$Output = "output",
    [Parameter(Mandatory = $true, ValueFromPipeline = $true, ValueFromPipelineByPropertyName = $true)]
    [string]$RifFile
)

if (-not (Test-Path $RifFile)) {
    Write-Error -Message "Could not find file '$RifFile'" -TargetObject $RifFile -Category ResourceUnavailable
    exit
}

$CURRENT_DIR = Get-Location
$RIF_PATH = Get-Item -Path $RifFile
$WORK_DIR = Split-Path $RIF_PATH

# Get hdr2exr command
if (Get-Command "hdr2exr" -errorAction SilentlyContinue) {
    $HDR2EXR_CMD = "hdr2exr"
}
elseif (Get-Command "$PSScriptRoot/../build/Release/bin/hdr2exr" -errorAction SilentlyContinue) {
    $HDR2EXR_CMD = "$PSScriptRoot/../build/Release/bin/hdr2exr"
}
elseif (Get-Command "$PSScriptRoot/../build/bin/hdr2exr" -errorAction SilentlyContinue) {
    $HDR2EXR_CMD = "$PSScriptRoot/../build/bin/hdr2exr"
}
elseif (Get-Command "$PSScriptRoot/../build/Debug/bin/hdr2exr" -errorAction SilentlyContinue) {
    $HDR2EXR_CMD = "$PSScriptRoot/../build/Debug/bin/hdr2exr"
}
else {
    $HDR2EXR_CMD = "hdr2exr" # Default
}

# We do not cache temporary files
$TMP_OCT = Get-ChildItem ([IO.Path]::GetTempFileName()) | Rename-Item -NewName { [IO.Path]::ChangeExtension($_, ".oct") } -PassThru

# Get number of logical processors
$thread_count = [System.Environment]::ProcessorCount

# Get content of the rif file
$RIF_TEXT = Get-Content $RIF_PATH -raw

# Extract all the scenes required for oconv
$SCENES = New-Object Collections.Generic.List[string]
if ($RIF_TEXT -match "materials\s*=\s*([^\n]+)") {
    $SCENES.Add([string]$matches[1].Trim())
}
if ($RIF_TEXT -match "scene\s*=\s*([^\n]+)") {
    $SCENES.Add([string]$matches[1].Trim())
}
# Write-Output $SCENES

# Get optional render arguments
$RENDER_ARGS = New-Object Collections.Generic.List[string]
if ($RIF_TEXT -match "render\s*=\s*([^\n]+)") {
    $RENDER_ARGS.Add([string]$matches[1].Trim())
}
# Write-Output $RENDER_ARGS

# Get the resolution, or use (our) default
$WIDTH = 512
$HEIGHT = 512
if ($RIF_TEXT -match "resolution\s*=\s*(\d+)\s+(\d+)") {
    $WIDTH = [int]$matches[1]
    $HEIGHT = [int]$matches[2]
}
# Write-Output $WIDTH $HEIGHT

# Get number of indirect bounces
$INDIRECT = 0
if ($RIF_TEXT -match "indirect\s*=\s*(\d+)") {
    $INDIRECT = [int]$matches[1]
}
# Write-Output $INDIRECT

# Get all the views and put it into an array
$VIEWS = New-Object Collections.Generic.List[string]
$all_matches = ([regex]"view\s*=\s*([^\n]+)").Matches($RIF_TEXT)
if ($all_matches.Count -gt 0) {
    foreach ($s in $all_matches) {
        $VIEWS.Add([string]$s.Groups[1].Value.Trim())
    }
}
# Write-Output $VIEWS

# Extract name of the output file
if ($Output -eq "output" -and $RIF_TEXT -match "picture\s*=\s*(\d+)") {
    $Output = [string]$matches[1].Trim()
}

# See
# https://floyd.lbl.gov/radiance/man_html/rpict.1.html
# https://floyd.lbl.gov/radiance/man_html/rtrace.1.html
# https://floyd.lbl.gov/radiance/man_html/vwrays.1.html
# for documentation
# The preselected parameters should ensure unbiased images from the first sample on
# Comparing to SPP based renderer (like Ignis) is difficult, as Radiance `rpict`, `rtrace` or `vwrays` do not have such an option

$SS = 0 #TODO: -ss N might be a good indicator for sample count (even while this is more like splitting per ray)
$AD = 800
$LW = 1 / $AD
$DEF = Get-Content "$PSScriptRoot/rtrace_default.txt"

# $RPARGS = "$DEF -ad $AD -lw $LW -ss $SS -ab $INDIRECT -x $WIDTH -y $HEIGHT $EXTRA_ARGS".Trim().Split()
$VWARGS = "-x $WIDTH -y $HEIGHT".Trim().Split()
$TRARGS = "-n $thread_count $DEF -ad $AD -lw $LW -ss $SS -ab $INDIRECT -ov -ffc -h+ $EXTRA_ARGS".Trim().Split()

# Write-Host $RPARGS
# Write-Host $VWARGS
# Write-Host $TRARGS

function RenderView {
    param (
        [string]$output_name
    )

    Write-Host "Rendering $output_name.exr" -ForegroundColor Cyan
    #rpict $VIEWS[0] $RPARGS $TMP_OCT > $output_name.hdr
    $view_params = (vwrays -d $VWARGS $VIEWS[0]).Split()
    vwrays -ff $VWARGS $VIEWS[0] | rtrace $TRARGS $view_params $TMP_OCT > "$output_name.hdr"
    & $HDR2EXR_CMD "$output_name.hdr" "$output_name.exr"
    Write-Host "Generated output $output_name.exr" -ForegroundColor Cyan
}

# Change to directory containing the input file if possible, to ensure correct loading of dependent files
Set-Location $WORK_DIR

try {
    $stats = Measure-Command {
        oconv $SCENES > $TMP_OCT

        if ($VIEWS.Count -eq 1) {
            RenderView "$Output"
        }
        else {
            $i = 0
            foreach ($view in $VIEWS) {
                RenderView ($Output + "_$i")
                $i += 1
            }
        }
    }

    $dur = $stats.TotalSeconds
    Write-Host "Took $dur seconds" -ForegroundColor Cyan
}
catch {
    Write-Host "Run failed:" -ForegroundColor Red
    Write-Host $_ -ForegroundColor Red
}

Set-Location $CURRENT_DIR
