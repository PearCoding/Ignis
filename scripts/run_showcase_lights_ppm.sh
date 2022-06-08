#!/bin/bash

# Run showcase for lights and photonmapping

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

source $SCRIPT_DIR/../source.sh

SCENE_DIR=$SCRIPT_DIR/../scenes/showcase
ARGS="--spp 1024 --technique photonmapper"
OUT=$SCRIPT_DIR/../build/out_showcase

lig_scenes=$( find ${SCENE_DIR}/lights -name "*.json" -type f -printf '%P\n' )

for scene in $lig_scenes; do
    if [ "$scene" = "base.json" ]; then
        continue
    fi
    igcli ${ARGS} -o $OUT/lig_${scene%.json}_ppm.exr $SCENE_DIR/lights/$scene
    exr2jpg $OUT/lig_${scene%.json}_ppm.exr
done
