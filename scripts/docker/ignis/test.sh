#! /bin/bash

cd build
# Run tests, but skip the resource heavy variants
ctest --output-on-failure -LE "integrator|multiple" . 
