#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

source $SCRIPT_DIR/../source.sh

SCENE_DIR=$SCRIPT_DIR/../scenes/showcase
ARGS="--spp 1024"
OUT=$SCRIPT_DIR/../build/out_showcase

mat_scenes=$( find ${SCENE_DIR}/bsdf -name "*.json" -type f -printf '%P\n' )
lig_scenes=$( find ${SCENE_DIR}/lights -name "*.json" -type f -printf '%P\n' )
tex_scenes=$( find ${SCENE_DIR}/texture -name "*.json" -type f -printf '%P\n' )

for scene in $mat_scenes; do
    if [ "$scene" = "base.json" ]; then
        continue
    fi
    igcli ${ARGS} -o $OUT/mat_${scene%.json}.exr $SCENE_DIR/bsdf/$scene
    exr2jpg $OUT/mat_${scene%.json}.exr
done

for scene in $lig_scenes; do
    if [ "$scene" = "base.json" ]; then
        continue
    fi
    igcli ${ARGS} -o $OUT/lig_${scene%.json}.exr $SCENE_DIR/lights/$scene
    exr2jpg $OUT/lig_${scene%.json}.exr
done

for scene in $tex_scenes; do
    if [ "$scene" = "base.json" ]; then
        continue
    fi
    igcli ${ARGS} -o $OUT/tex_${scene%.json}.exr $SCENE_DIR/texture/$scene
    exr2jpg $OUT/tex_${scene%.json}.exr
done
