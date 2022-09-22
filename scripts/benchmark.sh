#! /bin/bash

REGEX="\# ([0-9.]+)\/([0-9.]+)\/([0-9.]+)"

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

source $SCRIPT_DIR/../source.sh

quiet=false
scene=${SCRIPT_DIR}/../scenes/diamond_scene.json
executable=${BUILD_DIR}/bin/igcli
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
    -e)
        executable="$2"
        shift
        ;;
    -q) quiet=true ;;
    --)
        shift
        break
        ;;
    *) echo "$1 is not an option" ;;
    esac
    shift
done

args="--spp ${spp} -o _bench.exr ${scene} $@"

# Do some warm up iterations
if [ "$quiet" = false ]; then
    echo "Warm up..."
fi
for ((i = 0; i < $warmup_iterations; i++)); do
    if [ "$quiet" = false ]; then
        echo $(expr $i + 1)
    fi
    ${executable} ${args} >/dev/null
done

# Setup counters
min=0
med=0
max=0

# Start benchmark
if [ "$quiet" = false ]; then
    echo "Benchmark..."
fi

for ((i = 0; i < $num_iterations; i++)); do
    if [ "$quiet" = false ]; then
        echo $(expr $i + 1)
    fi
    output=$(${executable} ${args})
    output=$(echo "${output}" | grep -Po "$REGEX")
    output=${output:2}
    matches=(${output//\// })
    min=$(awk '{print $1+$2}' <<<"${min} ${matches[0]}")
    med=$(awk '{print $1+$2}' <<<"${med} ${matches[1]}")
    max=$(awk '{print $1+$2}' <<<"${max} ${matches[2]}")
done

# Compute and display result
min=$(awk '{print $1/$2}' <<<"${min} ${num_iterations}")
med=$(awk '{print $1/$2}' <<<"${med} ${num_iterations}")
max=$(awk '{print $1/$2}' <<<"${max} ${num_iterations}")

echo "# ${min}/${med}/${max} (min/med/max Msamples/s)"
