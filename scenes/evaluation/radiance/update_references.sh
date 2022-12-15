#!/bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
SCRIPT=${SCRIPT_DIR}/../../../scripts/run_rif.sh

cd $SCRIPT_DIR

function handle_scene {
    ${SCRIPT} $1.rif
}

handle_scene "plane-array-diffuse"
mv plane-array-diffuse.exr ref-plane-array-diffuse-rad.exr
handle_scene "plane-array-klems"
mv plane-array-klems_0.exr ref-plane-array-klems-front-rad.exr
mv plane-array-klems_1.exr ref-plane-array-klems-back-rad.exr
handle_scene "plane-array-tensortree"
mv plane-array-tensortree_0.exr ref-plane-array-tensortree-front-rad.exr
mv plane-array-tensortree_1.exr ref-plane-array-tensortree-back-rad.exr
handle_scene "sky-clear"
handle_scene "sky-cloudy"
handle_scene "sky-intermediate"
handle_scene "sky-perez1"
handle_scene "sky-uniform"
