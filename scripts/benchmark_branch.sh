#! /bin/bash

REGEX="\# ([0-9.]+)\/([0-9.]+)\/([0-9.]+)"

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
BUILD_DIR=${SCRIPT_DIR}/../build/Release
if [ ! -d "$BUILD_DIR" ]; then
    BUILD_DIR=${SCRIPT_DIR}/../build
fi

BENCH=$(realpath ${SCRIPT_DIR}/benchmark.sh)

exe1old=false
exe2old=false
bench_cpu=true
bench_gpu=true
scene=$(realpath ${SCRIPT_DIR}/../scenes/diamond_scene.json)
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
        executable1=$(realpath "$2")
        shift
        ;;
    -e2)
        executable2=$(realpath "$2")
        shift
        ;;
    -o1) exe1old=true ;;
    -o2) exe2old=true ;;
    --only-gpu) bench_cpu=false ;;
    --only-cpu) bench_gpu=false ;;
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

echo "Benchmarking ${executable1} and ${executable2}"

args=("-q --spp ${spp} -n ${num_iterations} -w ${warmup_iterations} -s ${scene} -- $@")
echo "Using arguments: ${args}"
echo "Using arguments: ${BENCH}"

if [ "$bench_cpu" = true ]; then
    if [ "$exe1old" = false ]; then
        B1_CPU=$(${BENCH} -e ${executable1} ${args} --cpu)
    else
        B1_CPU=$(${BENCH} -e ${executable1} ${args} --target generic)
    fi
    echo "B1 [CPU]     ${B1_CPU}"
fi
if [ "$bench_gpu" = true ]; then
    B1_GPU=$(${BENCH} -e ${executable1} ${args} --gpu)
    echo "B1 [GPU]     ${B1_GPU}" 
fi

if [ "$bench_cpu" = true ]; then
    if [ "$exe2old" = false ]; then
        B2_CPU=$(${BENCH} -e ${executable2} ${args} --cpu)
    else
        B2_CPU=$(${BENCH} -e ${executable2} ${args} --target generic)
    fi
    echo "B2 [CPU] ${B2_CPU}"
fi
if [ "$bench_gpu" = true ]; then
    B2_GPU=$(${BENCH} -e ${executable2} ${args} --gpu)
    echo "B2 [GPU] ${B2_GPU}" 
fi

function get_metrics() {
    local output=$(echo "$1" | grep -Po "$REGEX")
    output=${output:2}
    echo "${output//\// }"
}

# Display output
output=""
if [ "$bench_cpu" = true ]; then
    B1_CPU_METRIC=($(get_metrics "$B1_CPU"))
    B2_CPU_METRIC=($(get_metrics "$B2_CPU"))
    
    output+="CPU ${B1_CPU_METRIC[0]} ${B2_CPU_METRIC[0]} ${B1_CPU_METRIC[1]} ${B2_CPU_METRIC[1]} ${B1_CPU_METRIC[2]} ${B2_CPU_METRIC[2]}"
fi

if [ "$bench_gpu" = true ]; then
    B1_GPU_METRIC=($(get_metrics "$B1_GPU"))
    B2_GPU_METRIC=($(get_metrics "$B2_GPU"))
    
    if [ ! -z "${output}" ]; then
        output+="\n"
    fi
    output+="GPU ${B1_GPU_METRIC[0]} ${B2_GPU_METRIC[0]} ${B1_GPU_METRIC[1]} ${B2_GPU_METRIC[1]} ${B1_GPU_METRIC[2]} ${B2_GPU_METRIC[2]}"
fi

echo -e "${output}" | column --table --table-columns "Target,B1 Min,B2 Min,B1 Med,B2 Med,B1 Max,B2 Max"
