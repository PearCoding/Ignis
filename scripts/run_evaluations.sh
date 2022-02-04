#! /bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
BUILD_DIR=${SCRIPT_DIR}/../build/Release
if [ ! -d "$BUILD_DIR" ]; then
    BUILD_DIR=${SCRIPT_DIR}/../build
fi

scene_dir=${SCRIPT_DIR}/../scenes/evaluation
script=${SCRIPT_DIR}/../scenes/evaluation/MakeFigure.py
executable=${BUILD_DIR}/bin/igcli
spp=4096
output_dir=.

args="--spp ${spp} $@"

$executable ${args} -o ${output_dir}/cbox${spp}-d6.exr ${scene_dir}/cbox.json --gpu
$executable ${args} -o ${output_dir}/cbox${spp}-d1.exr ${scene_dir}/cbox-d1.json --gpu
$executable ${args} -o ${output_dir}/cbox${spp}-cpu-d6.exr ${scene_dir}/cbox.json --cpu
$executable ${args} -o ${output_dir}/cbox${spp}-cpu-d1.exr ${scene_dir}/cbox-d1.json --cpu

$executable ${args} -o ${output_dir}/plane${spp}-d6.exr ${scene_dir}/plane.json --gpu
$executable ${args} -o ${output_dir}/plane${spp}-d1.exr ${scene_dir}/plane-d1.json --gpu
$executable ${args} -o ${output_dir}/plane${spp}-cpu-d6.exr ${scene_dir}/plane.json --cpu
$executable ${args} -o ${output_dir}/plane${spp}-cpu-d1.exr ${scene_dir}/plane-d1.json --cpu

$executable ${args} -o ${output_dir}/room${spp}-d4.exr ${scene_dir}/room.json --gpu

python3 ${script} ${output}
