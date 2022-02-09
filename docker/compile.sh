#! /bin/bash

# We expect a minimum instruction based on the skylake processor
MARCH=skylake
CXX_FLAGS="-march=${MARCH} -mtune=generic"
CLANG_FLAGS="-march=${MARCH};-mtune=generic;-ffast-math"
# TODO: Attribute is fixed... this is bad
ARTIC_FLAGS="--host-triple;x86_64-linux-gnu;--host-cpu;znver2;--host-attr;+sse2,-tsxldtrk,+cx16,+sahf,-tbm,-avx512ifma,+sha,-gfni,-fma4,-vpclmulqdq,+prfchw,+bmi2,-cldemote,+fsgsbase,-ptwrite,-amx-tile,-uintr,+popcnt,-widekl,+aes,-avx512bitalg,-movdiri,+xsaves,-avx512er,-avxvnni,-avx512vnni,-amx-bf16,-avx512vpopcntdq,-pconfig,+clwb,-avx512f,+xsavec,+clzero,-pku,+mmx,-lwp,+rdpid,-xop,+rdseed,-waitpkg,-kl,-movdir64b,+sse4a,-avx512bw,+clflushopt,+xsave,-avx512vbmi2,+64bit,-avx512vl,-serialize,-hreset,-invpcid,-avx512cd,+avx,-vaes,-avx512bf16,+cx8,+fma,-rtm,+bmi,-enqcmd,+rdrnd,+mwaitx,+sse4.1,+sse4.2,+avx2,+fxsr,+wbnoinvd,+sse,+lzcnt,+pclmul,-prefetchwt1,+f16c,+ssse3,-sgx,-shstk,+cmov,-avx512vbmi,-amx-int8,+movbe,-avx512vp2intersect,+xsaveopt,-avx512dq,+adx,-avx512pf,+sse3"

mkdir -p /ignis/build
cd /ignis/build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DAnyDSL_runtime_DIR=/anydsl/runtime/build/share/anydsl/cmake -DCMAKE_INSTALL_PREFIX=/ignis-install -DCMAKE_CXX_FLAGS="${CXX_FLAGS}" -DIG_ARTIC_CLANG_FLAGS="${CLANG_FLAGS}" -DIG_ARTIC_FLAGS="${ARTIC_FLAGS}" -DIG_OPTIMIZE_FOR_NATIVE=OFF -DIG_WITH_DEVICE_CPU_GENERIC_ALWAYS=ON -DIG_WITH_DEVICE_CPU_BEST=OFF -DIG_WITH_DEVICE_CPU_AVX2=ON ..
cmake --build .
cat /ignis/build/src/backend/driver/driver_avx2.ll | head
cat /ignis/build/src/backend/driver/driver_avx2.ll | tail
cmake --install .
