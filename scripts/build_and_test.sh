#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"
BUILD_TYPE="${2:-Release}"

echo "=== [1/3] Configuring ClassicFPS Engine (${BUILD_TYPE}) in ${BUILD_DIR} ==="
cmake -S . -B "${BUILD_DIR}" -GNinja -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

echo "=== [2/3] Building ClassicFPS Engine and Tests ==="
cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo "=== [3/3] Running CTest Unit & Integration Suite ==="
ctest --test-dir "${BUILD_DIR}" --output-on-failure -V
