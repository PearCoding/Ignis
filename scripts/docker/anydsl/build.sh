#! /bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
DOCKER_USER=pearcoding

build_cpu=true
build_cuda=true

while [ -n "$1" ]; do
    case "$1" in
    --no-cpu) build_cpu=false ;;
    --no-cuda) build_cuda=false ;;
    --)
        shift
        break
        ;;
    *) echo "$1 is not an option" ;;
    esac
    shift
done

cd $SCRIPT_DIR

# Build all docker image flavours
if [ "$build_cpu" = true ]; then
    docker build -t $DOCKER_USER/anydsl:latest-cpu-jit -f Dockerfile-CPU-JIT .
fi

if [ "$build_cuda" = true ]; then
    docker build -t $DOCKER_USER/anydsl:latest-cuda-jit -f Dockerfile-CUDA-JIT .
fi