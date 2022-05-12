#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

source $SCRIPT_DIR/../../../source.sh

SCENE_DIFFUSE=$SCRIPT_DIR/plane_array_diffuse.json
SCENE_TENSOR=$SCRIPT_DIR/plane_array_tensortree.json
SCENE_KLEMS=$SCRIPT_DIR/plane_array_klems.json
ARGS="--spp 1024"
OUT=.

igcli ${ARGS} -o $OUT/output_diffuse_1.exr $SCENE_DIFFUSE
igcli ${ARGS} -o $OUT/output_diffuse_2.exr --eye 0.174 0.677 0.849 --dir 0.824 0.479 0.301 --up 0.343 -0.000 -0.939 $SCENE_DIFFUSE 
igcli ${ARGS} -o $OUT/output_diffuse_3.exr --eye 1.766 0.753 1.437 --dir -0.825 0.434 -0.362 --up -0.392 0.022 0.919 $SCENE_DIFFUSE

igcli ${ARGS} -o $OUT/output_tensor_1.exr $SCENE_TENSOR
igcli ${ARGS} -o $OUT/output_tensor_2.exr --eye 0.174 0.677 0.849 --dir 0.824 0.479 0.301 --up 0.343 -0.000 -0.939 $SCENE_TENSOR 
igcli ${ARGS} -o $OUT/output_tensor_3.exr --eye 1.766 0.753 1.437 --dir -0.825 0.434 -0.362 --up -0.392 0.022 0.919 $SCENE_TENSOR

igcli ${ARGS} -o $OUT/output_klems_1.exr $SCENE_KLEMS
igcli ${ARGS} -o $OUT/output_klems_2.exr --eye 0.174 0.677 0.849 --dir 0.824 0.479 0.301 --up 0.343 -0.000 -0.939 $SCENE_KLEMS 
igcli ${ARGS} -o $OUT/output_klems_3.exr --eye 1.766 0.753 1.437 --dir -0.825 0.434 -0.362 --up -0.392 0.022 0.919 $SCENE_KLEMS
