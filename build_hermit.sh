#!/bin/sh
export CC=cosmocc
export CXX=cosmoc++
cmake -DWAMR_BUILD_INTERP=1 -DWAMR_BUILD_FAST_INTERP=1 -B build
cmake --build build
cp src/cowsay.wasm build/main.wasm
cp src/hermit.json build/hermit.json
cd build
zip -r iwasm.com main.wasm hermit.json
