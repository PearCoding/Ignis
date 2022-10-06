#! /bin/bash

cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DAnyDSL_runtime_DIR=/anydsl/runtime/build/share/anydsl/cmake -DCMAKE_INSTALL_PREFIX=/ignis-install -DIG_OPTIMIZE_FOR_NATIVE=OFF ..
cmake --build .
cmake --install .
