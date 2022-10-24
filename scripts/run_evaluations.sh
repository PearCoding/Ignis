#! /bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

source $SCRIPT_DIR/../source.sh

scene_dir=${SCRIPT_DIR}/../scenes/evaluation
script=${SCRIPT_DIR}/MakeEvaluationFigure.py
spp=4096
output_dir=.

args="--spp ${spp} $@"

cmd_p=$(readlink -f $(which igcli))
echo "Evaluating command ${cmd_p}"

igcli ${args} -o ${output_dir}/cbox${spp}-d6.exr ${scene_dir}/cbox.json --gpu
igcli ${args} -o ${output_dir}/cbox${spp}-d1.exr ${scene_dir}/cbox-d1.json --gpu
igcli ${args} -o ${output_dir}/cbox${spp}-cpu-d6.exr ${scene_dir}/cbox.json --cpu
igcli ${args} -o ${output_dir}/cbox${spp}-cpu-d1.exr ${scene_dir}/cbox-d1.json --cpu

igcli ${args} -o ${output_dir}/plane${spp}-d6.exr ${scene_dir}/plane.json --gpu
igcli ${args} -o ${output_dir}/plane${spp}-d1.exr ${scene_dir}/plane-d1.json --gpu
igcli ${args} -o ${output_dir}/plane${spp}-cpu-d6.exr ${scene_dir}/plane.json --cpu
igcli ${args} -o ${output_dir}/plane${spp}-cpu-d1.exr ${scene_dir}/plane-d1.json --cpu

igcli ${args} -o ${output_dir}/room${spp}-d4.exr ${scene_dir}/room.json --gpu
igcli ${args} -o ${output_dir}/room${spp}-cpu-d4.exr ${scene_dir}/room.json --cpu

igcli ${args} -o ${output_dir}/plane-scale${spp}-d4.exr ${scene_dir}/plane-scale.json --gpu
igcli ${args} -o ${output_dir}/plane-scale${spp}-cpu-d4.exr ${scene_dir}/plane-scale.json --cpu

igcli ${args} -o ${output_dir}/volume${spp}-d12.exr ${scene_dir}/volume.json --gpu
igcli ${args} -o ${output_dir}/volume${spp}-cpu-d12.exr ${scene_dir}/volume.json --cpu

igcli ${args} -o ${output_dir}/env${spp}-d6.exr ${scene_dir}/env.json --gpu
igcli ${args} -o ${output_dir}/env4k${spp}-d6.exr ${scene_dir}/env4k.json --gpu
igcli ${args} -o ${output_dir}/env4kNoCDF${spp}-d6.exr ${scene_dir}/env4kNoCDF.json --gpu
igcli ${args} -o ${output_dir}/env${spp}-cpu-d6.exr ${scene_dir}/env.json --cpu
igcli ${args} -o ${output_dir}/env4k${spp}-cpu-d6.exr ${scene_dir}/env4k.json --cpu
igcli ${args} -o ${output_dir}/env4kNoCDF${spp}-cpu-d6.exr ${scene_dir}/env4kNoCDF.json --cpu

igcli ${args} -o ${output_dir}/multilight-uniform${spp}-d4.exr ${scene_dir}/multilight-uniform.json --gpu
igcli ${args} -o ${output_dir}/multilight-simple${spp}-d4.exr ${scene_dir}/multilight-simple.json --gpu
igcli ${args} -o ${output_dir}/multilight-hierarchy${spp}-d4.exr ${scene_dir}/multilight-hierarchy.json --gpu
igcli ${args} -o ${output_dir}/multilight-uniform${spp}-cpu-d4.exr ${scene_dir}/multilight-uniform.json --cpu
igcli ${args} -o ${output_dir}/multilight-simple${spp}-cpu-d4.exr ${scene_dir}/multilight-simple.json --cpu
igcli ${args} -o ${output_dir}/multilight-hierarchy${spp}-cpu-d4.exr ${scene_dir}/multilight-hierarchy.json --cpu

igcli ${args} -o ${output_dir}/point${spp}-d4.exr ${scene_dir}/point.json --gpu
igcli ${args} -o ${output_dir}/point${spp}-cpu-d4.exr ${scene_dir}/point.json --cpu

python3 ${script} ${output_dir}/ ${scene_dir}/
