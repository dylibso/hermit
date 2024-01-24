#!/bin/sh
export CC=x86_64-unknown-cosmo-cc
export CXX=x86_64-unknown-cosmo-c++
rm -rf build hermit-cli/build hermit-cli/target
mkdir build
cmake -DWAMR_BUILD_FAST_JIT=1 -B build
cmake --build build -j
