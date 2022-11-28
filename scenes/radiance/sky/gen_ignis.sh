#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

source $SCRIPT_DIR/../../../source.sh

ARGS="--spp 16384"
OUT=.

igcli ${ARGS} -o $OUT/ignis_uniform.exr $SCRIPT_DIR/uniform.json
igcli ${ARGS} -o $OUT/ignis_cloudy.exr $SCRIPT_DIR/cloudy.json
igcli ${ARGS} -o $OUT/ignis_clear.exr $SCRIPT_DIR/clear.json
igcli ${ARGS} -o $OUT/ignis_intermediate.exr $SCRIPT_DIR/intermediate.json
igcli ${ARGS} -o $OUT/ignis_perez1.exr $SCRIPT_DIR/perez1.json
