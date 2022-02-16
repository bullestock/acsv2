#!/bin/bash
mkdir dev
cd dev
cmake -D CMAKE_BUILD_TYPE=Debug -G Ninja ..
