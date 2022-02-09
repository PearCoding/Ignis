#! /bin/bash

# We expect a minimum instruction based on the skylake processor
MARCH=skylake
CXX_FLAGS="-march=${MARCH} -mtune=generic"
CLANG_FLAGS="-march=${MARCH};-mtune=generic;-ffast-math"

mkdir -p /ignis/build
cd /ignis/build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DAnyDSL_runtime_DIR=/anydsl/runtime/build/share/anydsl/cmake -DCMAKE_INSTALL_PREFIX=/ignis-install -DCMAKE_CXX_FLAGS="${CXX_FLAGS}" -DARTIC_CLANG_FLAGS="${CLANG_FLAGS}" -DIG_OPTIMIZE_FOR_NATIVE=OFF -DIG_WITH_DEVICE_CPU_GENERIC_ALWAYS=ON -DIG_WITH_DEVICE_CPU_BEST=OFF -DIG_WITH_DEVICE_CPU_AVX2=ON ..
cmake --build .
cat /ignis/build/src/backend/driver/driver_avx2.ll
cmake --install .
