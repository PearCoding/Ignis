#! /bin/bash

mkdir -p build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DAnyDSL_runtime_DIR=/anydsl/runtime/build/share/anydsl/cmake -DBUILD_TESTING=ON -DCMAKE_INSTALL_PREFIX=/ignis-install -DIG_OPTIMIZE_FOR_NATIVE=OFF -DIG_INSTALL_RUNTIME_DEPENDENCIES=OFF ..
cmake --build .
ctest --output-on-failure -LE "integrator|multiple" . 
