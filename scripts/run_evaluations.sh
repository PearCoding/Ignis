#! /bin/bash

REGEX="\# ([0-9.]+)\/([0-9.]+)\/([0-9.]+)"

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
BUILD_DIR=${SCRIPT_DIR}/../build/Release
if [ ! -d "$BUILD_DIR" ]; then
    BUILD_DIR=${SCRIPT_DIR}/../build
fi

scene1=${SCRIPT_DIR}/../scenes/evaluation/cbox.json
scene2=${SCRIPT_DIR}/../scenes/evaluation/cbox-d1.json
script=${SCRIPT_DIR}/../scenes/evaluation/MakeFigure.py
executable=${BUILD_DIR}/bin/igcli
spp=4096
output1=cbox${spp}.exr
output2=cbox${spp}-d1.exr

args="--spp ${spp} $@"

$executable ${args} -o ${output1} ${scene1}
$executable ${args} -o ${output2} ${scene2}

python3 ${script} ${output}
