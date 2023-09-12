#!/bin/sh
export CC=cosmocc
export CXX=cosmoc++
cmake -DWAMR_BUILD_INTERP=1 -DWAMR_BUILD_FAST_INTERP=1 -DWAMR_DISABLE_STACK_HW_BOUND_CHECK=1 -DWAMR_BUILD_AOT=0 -B build
cmake --build build
