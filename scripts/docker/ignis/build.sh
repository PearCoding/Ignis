#! /bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
DOCKER_USER=pearcoding

build_cpu=true
build_cuda=true
insource=false

while [ -n "$1" ]; do
    case "$1" in
    --no-cpu) build_cpu=false ;;
    --no-cuda) build_cuda=false ;;
    --insource) insource=true ;;
    --)
        shift
        break
        ;;
    *) echo "$1 is not an option" ;;
    esac
    shift
done

cd $SCRIPT_DIR

if [ "$insource" = false ]; then
    # Get branch name
    branch_name="$(git symbolic-ref HEAD 2>/dev/null)" ||
    branch_name="master"

    # Get a fresh Ignis
    if [ -d "tmp/ignis" ]; then
        cd tmp/ignis
        git pull
        cd $IG_DIR
    else
        mkdir tmp
        cd tmp
        git clone --branch="${branch_name}" https://github.com/PearCoding/Ignis.git ignis
        cd $SCRIPT_DIR
    fi
    RUN_DIR="."
    IG_DIR="tmp/ignis"
else
    RUN_DIR="../../.."
    IG_DIR="."
fi

# Build all docker image flavours
if [ "$build_cpu" = true ]; then
    echo "$RUN_DIR"
    docker build -t $DOCKER_USER/ignis:latest-cpu -f Dockerfile-CPU --build-arg IG_DIR="$IG_DIR" $RUN_DIR
fi

if [ "$build_cuda" = true ]; then
    docker build -t $DOCKER_USER/ignis:latest-cuda -f Dockerfile-CUDA --build-arg IG_DIR="$IG_DIR" $RUN_DIR
fi