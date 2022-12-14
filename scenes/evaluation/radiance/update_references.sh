#!/bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
SCRIPT=${SCRIPT_DIR}/../../../scripts/run_rif.sh

function handle_scene {
    ${SCRIPT} ${SCRIPT_DIR}/$1.rif
}

handle_scene "plane-array-diffuse"
handle_scene "plane-array-klems"
handle_scene "plane-array-tensortree"
handle_scene "sky-clear"
handle_scene "sky-cloudy"
handle_scene "sky-intermediate"
handle_scene "sky-perez1"
handle_scene "sky-uniform"
