#! /bin/bash

mkdir -p build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DAnyDSL_runtime_DIR=/anydsl/runtime/build/share/anydsl/cmake -DBUILD_TESTING=ON -DCMAKE_INSTALL_PREFIX=/ignis-install -DIG_OPTIMIZE_FOR_NATIVE=OFF -DIG_INSTALL_RUNTIME_DEPENDENCIES=OFF ..
cmake --build .
# FIXME: ignis_test_artic should be tested, but currently the CUDA device stuff is getting in the way
ctest --output-on-failure -E "(ignis_test_artic)|(ignis_test_integrator)|(ignis_test_multiple_runtimes)" .
