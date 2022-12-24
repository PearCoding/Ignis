#!/usr/bin/env bash
set -eu

git clone https://github.com/AnyDSL/anydsl.git anydsl
cp config.sh anydsl/config.sh

# Setup
cd anydsl
bash ./setup.sh

# Cleanup
[ ! -d llvm_build ] || rm -r llvm_build
[ ! -d llvm-project ] || rm -r llvm-project
find */build/src -maxdepth 0 -type d -exec rm -rf {} +