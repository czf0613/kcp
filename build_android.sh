#!/bin/bash
set -e
cd "$(dirname "$0")"

rm -rf build || true
mkdir -p build

ANDROID_NDK=$HOME/Library/Android/sdk/ndk/28.0.13004108

cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
-DANDROID_ABI=arm64-v8a \
-DANDROID_NATIVE_API_LEVEL=23 \
..

cmake --build . --config Release