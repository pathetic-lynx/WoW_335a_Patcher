#!/usr/bin/env bash
# Requires: mingw-w64 (pacman -S mingw-w64-gcc  /  apt install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64)
set -euo pipefail

BUILD_DIR="build-windows"

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE=cmake/windows-x86_64-toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo ""
echo "Artifacts:"
find "$BUILD_DIR" -maxdepth 1 -name "*.exe" | sort
