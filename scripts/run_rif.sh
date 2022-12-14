#!/bin/bash
# Basic script running Radiance in a "Ignis"-compatible way and generating an EXR (instead of a HDR)
# Use `run_rif.sh RIF_FILE`
# In contrary to rad *.rif, this one does not cache files nor does is share the same command line parameters!
# In contrary to run_rad.sh, this one extracts most information in the given rif file.
# The script ignores QUALITY, DETAIL, VARIABILITY and EXPOSURE however.
# The intended use case is limited to short renderings for comparison purposes
# `oconv` and `rpict` (both Radiance tools) have to be available in the current scope

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
source $SCRIPT_DIR/../source.sh

TMP_OCT=$(mktemp).oct
TMP_HDR=$(mktemp).hdr

INPUT="$1"

input_file=$(cat "$INPUT")

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

RENDER_ARGS=""
regex="render[[:blank:]]*=[[:blank:]]*([^
]+)"
if [[ $input_file =~ $regex ]]; then
    RENDER_ARGS="${BASH_REMATCH[1]}"
fi

WIDTH=512
HEIGHT=512
regex="RESOLUTION[[:blank:]]*=[[:blank:]]*([[:digit:]]+)[[:blank:]]+([[:digit:]]+)"
if [[ $input_file =~ $regex ]]; then
    WIDTH="${BASH_REMATCH[1]}"
    HEIGHT="${BASH_REMATCH[2]}"
fi

INDIRECT=0
regex="INDIRECT[[:blank:]]*=[[:blank:]]*([[:digit:]]+)"
if [[ $input_file =~ $regex ]]; then
    INDIRECT="${BASH_REMATCH[1]}"
fi

VIEWS=()
regex="view[[:blank:]]*=[[:blank:]]*([^
]+)"
function handle_view {
  msg="$1"
  if [[ "$msg" =~ $regex ]] ; then
    VIEWS+=("${BASH_REMATCH[1]}")
    handle_view "${msg/${BASH_REMATCH[0]}/}"
  fi
}
handle_view "$input_file"

OUTPUT="output"
regex="PICTURE[[:blank:]]*=[[:blank:]]*([^
]+)"
if [[ $input_file =~ $regex ]]; then
    OUTPUT="${BASH_REMATCH[1]}"
fi

# See https://floyd.lbl.gov/radiance/man_html/rpict.1.html for documentation
# The preselected parameters should ensure unbiased images from the first sample on
# Comparing to SPP based renderer (like Ignis) is difficult, as Radiance `rpict` does not have such an option

SS=1 #TODO: -ss N might be a good indicator for sample count (even while this is more like splitting per ray)
AD=800
LW=$(awk "BEGIN {print 1/$AD}")
DEF=$(cat "$SCRIPT_DIR/rpict_default.txt")
ARGS="$DEF -ad $AD -lw $LW -ss $SS -ab $INDIRECT -x $WIDTH -y $HEIGHT $EXTRA_ARGS"

oconv $SCENES > $TMP_OCT || exit 1

if [[ ${#VIEWS[@]} == 1 ]]; then
    rpict ${VIEWS[0]} $ARGS $TMP_OCT > $TMP_HDR || exit 1
    hdr2exr "$TMP_HDR" "$OUTPUT.exr"
else
    for i in ${!VIEWS[@]}; do
        view_output="${OUTPUT%%.*}_$i" # Expand given output filename
        rpict ${VIEWS[$i]} $ARGS $TMP_OCT > $TMP_HDR || exit 1
        hdr2exr "$TMP_HDR" "$view_output.exr"
    done
fi
