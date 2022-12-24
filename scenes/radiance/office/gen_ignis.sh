#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

source $SCRIPT_DIR/../../../source.sh

SCENE=$SCRIPT_DIR/office_scene.json
ARGS="--spp 16384"
OUT=.

igcli ${ARGS} -o $OUT/output_ignis.exr $SCENE
