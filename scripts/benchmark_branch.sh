#! /bin/bash

REGEX="\# ([0-9.]+)\/([0-9.]+)\/([0-9.]+)"

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
BUILD_DIR=${SCRIPT_DIR}/../build/Release
if [ ! -d "$BUILD_DIR" ]; then
    BUILD_DIR=${SCRIPT_DIR}/../build
fi

BENCH=${SCRIPT_DIR}/benchmark.sh

scene=${SCRIPT_DIR}/../scenes/diamond_scene.json
executable1=${BUILD_DIR}/bin/igcli
executable2=${BUILD_DIR}/bin/igcli
spp=64
num_iterations=10
warmup_iterations=2

while [ -n "$1" ]; do
    case "$1" in
    --spp)
        spp="$2"
        shift
        ;;
    -s)
        scene="$2"
        shift
        ;;
    -n)
        num_iterations="$2"
        shift
        ;;
    -w)
        warmup_iterations="$2"
        shift
        ;;
    -e1)
        executable1="$2"
        shift
        ;;
    -e2)
        executable2="$2"
        shift
        ;;
    --)
        shift
        break
        ;;
    *) echo "$1 is not an option" ;;
    esac
    shift
done

if [[ "${executable1}" == "${executable2}" ]]; then
    echo "Does not make sense to benchmark the same executable with itself"
    exit
fi

args=("-q --spp ${spp} -n ${num_iterations} -w ${warmup_iterations} -s ${scene} -- $@")

B1_GENERIC=$(${BENCH} -e ${executable1} ${args[@]} --target generic)
echo "B1 [Generic] ${B1_GENERIC}"
B1_CPU=$(${BENCH} -e ${executable1} ${args[@]} --cpu)
echo "B1 [CPU]     ${B1_CPU}"
B1_GPU=$(${BENCH} -e ${executable1} ${args[@]} --gpu)
echo "B1 [GPU]     ${B1_GPU}" 

B2_GENERIC=$(${BENCH} -e ${executable2} ${args[@]} --target generic)
echo "B2 [Generic] ${B2_GENERIC}"
B2_CPU=$(${BENCH} -e ${executable2} ${args[@]} --cpu)
echo "B2 [CPU]     ${B2_CPU}"
B2_GPU=$(${BENCH} -e ${executable2} ${args[@]} --gpu)
echo "B2 [GPU]     ${B2_GPU}" 

function get_metrics() {
    local output=$(echo "$1" | grep -Po "$REGEX")
    output=${output:2}
    echo "${output//\// }"
}

# Display output
B1_GENERIC_METRIC=($(get_metrics "$B1_GENERIC"))
B1_CPU_METRIC=($(get_metrics "$B1_CPU"))
B1_GPU_METRIC=($(get_metrics "$B1_GPU"))
B2_GENERIC_METRIC=($(get_metrics "$B2_GENERIC"))
B2_CPU_METRIC=($(get_metrics "$B2_CPU"))
B2_GPU_METRIC=($(get_metrics "$B2_GPU"))

echo -e "Generic ${B1_GENERIC_METRIC[0]} ${B2_GENERIC_METRIC[0]} ${B1_GENERIC_METRIC[1]} ${B2_GENERIC_METRIC[1]} ${B1_GENERIC_METRIC[2]} ${B2_GENERIC_METRIC[2]}\nCPU ${B1_CPU_METRIC[0]} ${B2_CPU_METRIC[0]} ${B1_CPU_METRIC[1]} ${B2_CPU_METRIC[1]} ${B1_CPU_METRIC[2]} ${B2_CPU_METRIC[2]}\nGPU ${B1_GPU_METRIC[0]} ${B2_GPU_METRIC[0]} ${B1_GPU_METRIC[1]} ${B2_GPU_METRIC[1]} ${B1_GPU_METRIC[2]} ${B2_GPU_METRIC[2]}" | column --table --table-columns "Target,B1 Min,B2 Min,B1 Med,B2 Med,B1 Max,B2 Max"
