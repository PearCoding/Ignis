#!/bin/bash
# Basic script running Radiance in a "Ignis"-compatible way and generating an EXR (instead of a HDR)
# Use `run_rad.sh RPICT_PARAMETERS.. -- SCENES.. OUTPUT`
# In contrary to rad *.rif, this one does not cache files nor does is share the same command line parameters!
# The intended use case is limited to short renderings for comparison purposes
# `oconv` and `rpict` (both Radiance tools) have to be available

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
source $SCRIPT_DIR/../source.sh

TMP_OCT=$(mktemp).oct
TMP_HDR=$(mktemp).hdr
SCENES=()
ARGS=()

while [ -n "$1" ]; do
    case "$1" in
    --)
        shift
        break
        ;;
    *) 
        ARGS+=("$1")
        shift
        ;;
    esac
done

while [ -n "$1" ]; do
    SCENES+=("$1")
    shift
done

# Use last scene entry as output
OUTPUT=${SCENES[${#SCENES[@]}-1]}
unset SCENES[${#SCENES[@]}-1]

AD=800
LW=$(awk "BEGIN {print 1/$AD}")

# See https://floyd.lbl.gov/radiance/man_html/rpict.1.html for documentation
# The preselected parameters should ensure unbiased images from the first sample on
# Comparing to SPP based renderer (like Ignis) is difficult, as Radiance `rpict` does not have such an option

oconv ${SCENES[@]} > $TMP_OCT || exit 1
rpict -t 10 -u+ -ps 1 -aa 0 -as 0 -av 0 0 0 -aw 0 -pt 0 -ss 0 -st 0 -as 128 -ad $AD -lw $LW -dt 0 -dp 0 -dc 0 -ar 0 -lr -16 ${ARGS[@]} $TMP_OCT > $TMP_HDR || exit 1
hdr2exr "$TMP_HDR" "$OUTPUT"
