#!/usr/bin/env bash
set -eu

[ ! -d llvm_build ] || rm -r llvm_build
[ ! -d llvm-project ] || rm -r llvm-project
find */build/src -maxdepth 0 -type d -exec rm -rf {} +