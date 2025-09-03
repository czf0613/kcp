#!/bin/bash
set -e
cd "$(dirname "$0")"

rm -rf build || true
mkdir -p build

cd build
cmake ..
cmake --build . --config Release