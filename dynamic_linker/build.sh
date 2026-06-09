#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake \
  -DCMAKE_C_COMPILER=aarch64-none-linux-gnu-gcc \
  -DCMAKE_CXX_COMPILER=aarch64-none-linux-gnu-g++ \
  ..
make -j"$(nproc)"
