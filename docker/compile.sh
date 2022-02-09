#! /bin/bash

# We expect a minimum instruction based on the skylake processor
MARCH=skylake
CXX_FLAGS="-march=${MARCH} -mtune=generic"
CLANG_FLAGS="-march=${MARCH};-mtune=generic;-ffast-math"

mkdir -p /ignis/build
cd /ignis/build
echo "int main(void) { return 0; }" >> test.cpp
/anydsl/llvm_install/bin/clang ${CLANG_FLAGS//;/ } -S -emit-llvm test.cpp -o test.ll

TARGET_TRIPLE=$(cat test.ll | sed -n "s/^.*target triple = \s*\"\(\S*\)\".*$/\1/p")
TARGET_ATTR=$(cat test.ll | sed -n "s/^.*\"target-features\"=\"\(\S*\)\".*$/\1/p")

ARTIC_FLAGS="--host-triple;${TARGET_TRIPLE};--host-cpu;${MARCH};--host-attr;${TARGET_ATTR}"

cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DAnyDSL_runtime_DIR=/anydsl/runtime/build/share/anydsl/cmake -DCMAKE_INSTALL_PREFIX=/ignis-install -DCMAKE_CXX_FLAGS="${CXX_FLAGS}" -DIG_ARTIC_CLANG_FLAGS="${CLANG_FLAGS}" -DIG_ARTIC_FLAGS="${ARTIC_FLAGS}" -DIG_OPTIMIZE_FOR_NATIVE=OFF -DIG_WITH_DEVICE_CPU_GENERIC_ALWAYS=ON -DIG_WITH_DEVICE_CPU_BEST=OFF -DIG_WITH_DEVICE_CPU_AVX2=ON ..
cmake --build .
cat /ignis/build/src/backend/driver/driver_avx2.ll | head
cat /ignis/build/src/backend/driver/driver_avx2.ll | tail
cmake --install .
