#! /bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

source $SCRIPT_DIR/../source.sh

scene_dir=${SCRIPT_DIR}/../scenes/evaluation
script=${SCRIPT_DIR}/MakeEvaluationFigure.py
spp=4096
output_dir=.

args="--spp ${spp} $@"

eval_cpu=true
eval_gpu=true

while [ -n "$1" ]; do
    case "$1" in
    --no-cpu) eval_cpu=false ;;
    --no-gpu) eval_gpu=false ;;
    --)
        shift
        break
        ;;
    *) echo "$1 is not an option" ;;
    esac
    shift
done

cmd_p=$(readlink -f $(which igcli))
echo "Evaluating command ${cmd_p}"

evaluate () {
    if [ "$eval_gpu" = true ]; then
        igcli ${args} -o $1 $2 --gpu
    fi

    if [ "$eval_cpu" = true ]; then
        local filename="$(basename -- $1)"
        local extension="${filename##*.}"
        local cpu_file="$(dirname -- $1)/$(basename -s .${extension} -- $1)-cpu.${extension}"
        igcli ${args} -o $cpu_file $2 --cpu
    fi
}

evaluate ${output_dir}/cbox${spp}-d6.exr ${scene_dir}/cbox.json
evaluate ${output_dir}/cbox${spp}-d1.exr ${scene_dir}/cbox-d1.json

evaluate ${output_dir}/plane${spp}-d6.exr ${scene_dir}/plane.json
evaluate ${output_dir}/plane${spp}-d1.exr ${scene_dir}/plane-d1.json

evaluate ${output_dir}/room${spp}-d4.exr ${scene_dir}/room.json

evaluate ${output_dir}/plane-scale${spp}-d4.exr ${scene_dir}/plane-scale.json

evaluate ${output_dir}/volume${spp}-d12.exr ${scene_dir}/volume.json

evaluate ${output_dir}/env${spp}-d6.exr ${scene_dir}/env.json
evaluate ${output_dir}/env4k${spp}-d6.exr ${scene_dir}/env4k.json
evaluate ${output_dir}/env4kNoCDF${spp}-d6.exr ${scene_dir}/env4kNoCDF.json

evaluate ${output_dir}/multilight-uniform${spp}-d4.exr ${scene_dir}/multilight-uniform.json
evaluate ${output_dir}/multilight-simple${spp}-d4.exr ${scene_dir}/multilight-simple.json
evaluate ${output_dir}/multilight-hierarchy${spp}-d4.exr ${scene_dir}/multilight-hierarchy.json

evaluate ${output_dir}/point${spp}-d4.exr ${scene_dir}/point.json

evaluate ${output_dir}/sphere-light-ico${spp}-d4.exr ${scene_dir}/sphere-light-ico.json
evaluate ${output_dir}/sphere-light-ico-nopt${spp}-d4.exr ${scene_dir}/sphere-light-ico-nopt.json
evaluate ${output_dir}/sphere-light-uv${spp}-d4.exr ${scene_dir}/sphere-light-uv.json
evaluate ${output_dir}/sphere-light-pure${spp}-d4.exr ${scene_dir}/sphere-light-pure.json

python3 ${script} ${output_dir}/ ${scene_dir}/
