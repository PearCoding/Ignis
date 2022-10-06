#! /bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
DOCKER_USER=pearcoding

cd $SCRIPT_DIR

# Build all docker image flavours
docker build -t $DOCKER_USER/ignis:latest-cpu -f Dockerfile-CPU ../..
docker build -t $DOCKER_USER/ignis:latest-cuda -f Dockerfile-CUDA ../..