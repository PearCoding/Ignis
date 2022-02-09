#! /bin/bash

# We expect a minimum instruction based on the skylake processor
MARCH=skylake
CXX_FLAGS="-march=${MARCH} -mtune=generic"
CLANG_FLAGS="-march=${MARCH};-mtune=generic;-ffast-math"

# --host-attr extracted from llvm for MARCH architecture. Could be handled automatically, maybe?
ARTIC_FLAGS="--host-triple;x86_64-linux-gnu;--host-cpu;skylake;--host-attr;+adx,+aes,+avx,+avx2,+bmi,+bmi2,+clflushopt,+cx16,+cx8,+f16c,+fma,+fsgsbase,+fxsr,+invpcid,+lzcnt,+mmx,+movbe,+pclmul,+popcnt,+prfchw,+rdrnd,+rdseed,+sahf,+sgx,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave,+xsavec,+xsaveopt,+xsaves"

mkdir -p /ignis/build
cd /ignis/build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DAnyDSL_runtime_DIR=/anydsl/runtime/build/share/anydsl/cmake -DCMAKE_INSTALL_PREFIX=/ignis-install -DCMAKE_CXX_FLAGS="${CXX_FLAGS}" -DIG_ARTIC_CLANG_FLAGS="${CLANG_FLAGS}" -DIG_ARTIC_FLAGS="${ARTIC_FLAGS}" -DIG_OPTIMIZE_FOR_NATIVE=OFF -DIG_WITH_DEVICE_CPU_GENERIC_ALWAYS=ON -DIG_WITH_DEVICE_CPU_BEST=OFF -DIG_WITH_DEVICE_CPU_AVX2=ON ..
cmake --build .
cat /ignis/build/src/backend/driver/driver_avx2.ll | head
cat /ignis/build/src/backend/driver/driver_avx2.ll | tail
cmake --install .
