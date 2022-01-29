#!/bin/bash
mkdir build
cd build
cmake -D CMAKE_C_COMPILER=arm-opi-linux-gnueabihf-gcc -D CMAKE_CXX_COMPILER=arm-opi-linux-gnueabihf-gcc -G Ninja ..
