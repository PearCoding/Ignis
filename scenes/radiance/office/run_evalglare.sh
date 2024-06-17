#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

source $SCRIPT_DIR/../../../source.sh

VIEW="-vta -vp 18.5 2 -6 -vd -0.853 -0.174 0.492 -vu -0.150 0.985 0.087 -vh 180 -vv 180 -vo 0 -va 0 -vs 0 -vl 0"

igutil convert output_ignis.exr output_ignis.hdr
echo "Ignis"
evalglare ${VIEW} -s -d -c ignis.hdr output_ignis.hdr 
echo "Radiance"
evalglare -s -d -c radiance.hdr output_1.hdr 
