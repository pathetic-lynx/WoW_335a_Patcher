#!/usr/bin/env bash
set -e
cmake -S . -B build-windows \
    -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw64.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows --parallel
echo "Output: build-windows/WoW_335a_Patcher.exe"
