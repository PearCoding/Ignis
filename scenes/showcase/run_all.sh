#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

CONV=$SCRIPT_DIR/../../build/Release/bin/exr2png
EXE=$SCRIPT_DIR/../../build/Release/bin/igcli
ARGS="--spp 1024"
CLI="${EXE} ${ARGS}"
OUT=out

scenes=$( find ${SCRIPT_DIR} -name "*.json" -type f -printf '%P\n' )

for scene in $scenes; do
    if [ "$scene" = "base.json" ]; then
        continue
    fi
    $CLI -o $OUT/${scene%.json}.exr $SCRIPT_DIR/$scene
done


find $OUT -name '*.exr' -exec $CONV {} \;