#!/bin/bash
set -e

export OBOE_LOC=~/oboe
git clone --depth 1 -b 1.3-stable https://github.com/google/oboe "$OBOE_LOC"

ls *

cmake \​
    -DANDROID_STL=c++_shared \​
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_SDK_ROOT/ndk-bundle/build/cmake/android.toolchain.cmake" \
    -DOBOE_SOURCE="$OBOE_LOC" \
    -DALSOFT_REQUIRE_OBOE=ON \
    -DALSOFT_REQUIRE_OPENSL=ON \
    -DALSOFT_EMBED_HRTF_DATA=YES \
    .
cmake --build . --clean-first

ls *

