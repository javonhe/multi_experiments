#!/bin/bash
rm -rf CMakeCache.txt CMakeFiles cmake_install.cmake Makefile
cmake -DCMAKE_C_COMPILER=aarch64-none-linux-gnu-gcc -DCMAKE_CXX_COMPILER=aarch64-none-linux-gnu-g++ .
make