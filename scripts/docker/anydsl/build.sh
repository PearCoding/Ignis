#! /bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
DOCKER_USER=pearcoding

cd $SCRIPT_DIR

# Get AnyDSL
ANYDSL_DIR="${SCRIPT_DIR}/tmp/anydsl"
if [ -d "$ANYDSL" ]; then
    cd $ANYDSL_DIR
    git pull
    cd $SCRIPT_DIR
else
    mkdir tmp
    cd tmp
    git clone https://github.com/AnyDSL/anydsl.git anydsl
    cd $SCRIPT_DIR
fi

# Copy given config.sh to tmp/anydsl
cp config.sh tmp/anydsl/config.sh
cp cleanup.sh tmp/anydsl/cleanup.sh

# Build all docker image flavours
docker build -t $DOCKER_USER/anydsl:latest-cpu-jit -f Dockerfile-CPU-JIT .
docker build -t $DOCKER_USER/anydsl:latest-cuda-jit -f Dockerfile-CUDA-JIT .