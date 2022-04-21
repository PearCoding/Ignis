#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

EXE=$SCRIPT_DIR/../../../build/Release/bin/igcli
SCENE=$SCRIPT_DIR/../../plane_array_tensortree.json
ARGS="--spp 1024"
CLI="${EXE} ${ARGS}"
OUT=out

$CLI -o $OUT/output_1.exr $SCENE
$CLI -o $OUT/output_2.exr --eye 0.174 0.677 0.849 --dir 0.824 0.479 0.301 --up 0.343 -0.000 -0.939 $SCENE 
$CLI -o $OUT/output_3.exr --eye 1.766 0.753 1.437 --dir -0.825 0.434 -0.362 --up -0.392 0.022 0.919 $SCENE
