#! /bin/bash

REGEX="\# ([0-9.]+)\/([0-9.]+)\/([0-9.]+)"

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
BUILD_DIR=${SCRIPT_DIR}/../build/Release
if [ ! -d "$BUILD_DIR" ]; then
    BUILD_DIR=${SCRIPT_DIR}/../build
fi

scene_dir=${SCRIPT_DIR}/../scenes/evaluation
script=${SCRIPT_DIR}/../scenes/evaluation/MakeFigure.py
executable=${BUILD_DIR}/bin/igcli
spp=4096
output_pre=cbox${spp}

args="--spp ${spp} $@"

$executable ${args} -o ${output_pre}.exr ${scene_dir}/cbox.json --gpu
$executable ${args} -o ${output_pre}-d1.exr ${scene_dir}/cbox-d1.json --gpu
# $executable ${args} -o ${output_pre}-cpu.exr ${scene_dir}/cbox.json --cpu
# $executable ${args} -o ${output_pre}-cpu-d1.exr ${scene_dir}/cbox-d1.json --cpu

python3 ${script} ${output}
