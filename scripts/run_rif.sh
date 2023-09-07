#!/bin/bash
# Basic script running Radiance in a "Ignis"-compatible way and generating an EXR (instead of a HDR)
# Use `run_rif.sh RIF_FILE`
# In contrary to rad *.rif, this one does not cache files nor does is share the same command line parameters!
# In contrary to run_rad.sh, this one extracts most information in the given rif file.
# The script ignores QUALITY, DETAIL, VARIABILITY and EXPOSURE however.
# The intended use case is limited to short renderings for comparison purposes
# `oconv`, `rpict`, `rtrace` and `vwrays` (all Radiance tools) have to be available in the current scope

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
source $SCRIPT_DIR/../source.sh -- 

# We do not cache temporary files
TMP_OCT=$(mktemp).oct
TMP_HDR=$(mktemp).hdr

INPUT="$1"
input_file=$(cat "$INPUT")

# Change to directory containing the input file if possible, to ensure correct loading of dependent files
parent_dir="$(dirname "$INPUT")"
if [[ $parent_dir != '' ]]; then
    cd $parent_dir
fi

# Get number of available threads on the system
thread_count=$(nproc --all 2>/dev/null || echo 8)
# thread_count=$(( thread_count > 16 ? 16 : thread_count ))

# Extract all the scenes required for oconv
SCENES=""
regex="materials[[:blank:]]*=[[:blank:]]*([^
]+)"
if [[ $input_file =~ $regex ]]; then
    SCENES+="${BASH_REMATCH[1]} "
fi

regex="scene[[:blank:]]*=[[:blank:]]*([^
]+)"
if [[ $input_file =~ $regex ]]; then
    SCENES+="${BASH_REMATCH[1]}"
fi

# Get optional render arguments
RENDER_ARGS=""
regex="render[[:blank:]]*=[[:blank:]]*([^
]+)"
if [[ $input_file =~ $regex ]]; then
    RENDER_ARGS="${BASH_REMATCH[1]}"
fi

# Get the resolution, or use (our) default
WIDTH=512
HEIGHT=512
regex="RESOLUTION[[:blank:]]*=[[:blank:]]*([[:digit:]]+)[[:blank:]]+([[:digit:]]+)"
if [[ $input_file =~ $regex ]]; then
    WIDTH="${BASH_REMATCH[1]}"
    HEIGHT="${BASH_REMATCH[2]}"
fi

# Get number of indirect bounces
INDIRECT=0
regex="INDIRECT[[:blank:]]*=[[:blank:]]*([[:digit:]]+)"
if [[ $input_file =~ $regex ]]; then
    INDIRECT="${BASH_REMATCH[1]}"
fi

# Get all the views and put it into an array
VIEWS=()
regex="view[[:blank:]]*=[[:blank:]]*([^
]+)"
function handle_view {
    msg="$1"
    if [[ "$msg" =~ $regex ]]; then
        VIEWS+=("${BASH_REMATCH[1]}")
        handle_view "${msg/${BASH_REMATCH[0]}/}"
    fi
}
handle_view "$input_file"

# Extract name of the output file
OUTPUT="output"
regex="PICTURE[[:blank:]]*=[[:blank:]]*([^
]+)"
if [[ $input_file =~ $regex ]]; then
    OUTPUT="${BASH_REMATCH[1]}"
fi

# See
# https://floyd.lbl.gov/radiance/man_html/rpict.1.html
# https://floyd.lbl.gov/radiance/man_html/rtrace.1.html
# https://floyd.lbl.gov/radiance/man_html/vwrays.1.html
# for documentation
# The preselected parameters should ensure unbiased images from the first sample on
# Comparing to SPP based renderer (like Ignis) is difficult, as Radiance `rpict`, `rtrace` or `vwrays` do not have such an option

SS=0 #TODO: -ss N might be a good indicator for sample count (even while this is more like splitting per ray)
AD=800
LW=$(awk "BEGIN {print 1/$AD}")
DEF=$(cat "$SCRIPT_DIR/rtrace_default.txt")

RPARGS="$DEF -ad $AD -lw $LW -ss $SS -ab $INDIRECT -x $WIDTH -y $HEIGHT $EXTRA_ARGS"
VWARGS="-x $WIDTH -y $HEIGHT"
TRARGS="-n $thread_count $DEF -ad $AD -lw $LW -ss $SS -ab $INDIRECT -ov -ffc -h+ $EXTRA_ARGS"

echo $thread_count
start=`date +%s.%N`

oconv $SCENES > $TMP_OCT || exit 1

if [[ ${#VIEWS[@]} == 1 ]]; then
    echo "Rendering $OUTPUT.exr"
    #rpict ${VIEWS[0]} $RPARGS $TMP_OCT > $TMP_HDR || exit 1
    vwrays -ff $VWARGS ${VIEWS[0]} | rtrace $TRARGS $(vwrays -d $VWARGS ${VIEWS[0]}) $TMP_OCT >$TMP_HDR || exit 1
    hdr2exr "$TMP_HDR" "$OUTPUT.exr"
    echo "Generated output $OUTPUT.exr"
else
    for i in ${!VIEWS[@]}; do
        view_output="${OUTPUT%%.*}_$i" # Expand given output filename
        echo "[$i] Rendering $view_output.exr"
        #rpict ${VIEWS[$i]} $RPARGS $TMP_OCT > $TMP_HDR || exit 1
        vwrays $VWARGS -ff ${VIEWS[$i]} | rtrace $TRARGS $(vwrays -d $VWARGS ${VIEWS[$i]}) $TMP_OCT >$TMP_HDR || exit 1
        hdr2exr "$TMP_HDR" "$view_output.exr"
        echo "Generated output $view_output.exr"
    done
fi

end=`date +%s.%N`
dur=$( echo "$end - $start" | bc -l )

echo "Took ${dur} seconds"
