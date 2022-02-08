#! /bin/bash

mkdir -p /ignis/build
cd /ignis/build
cmake -G Ninja -DAnyDSL_runtime_DIR=/anydsl/runtime/build/share/anydsl/cmake ..
cmake --build .
