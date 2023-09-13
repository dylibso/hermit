#!/bin/sh
export CC=cosmocc
export CXX=cosmoc++
cmake -DWAMR_BUILD_INTERP=1 -DWAMR_BUILD_FAST_INTERP=1 -B build
cmake --build build
