#!/bin/bash
set -e

cmake_build () {
  ANDROID_ABI=$1
  mkdir -p build-$ANDROID_ABI
  mkdir -p lib/$ANDROID_ABI
  cd build-$ANDROID_ABI
  cmake \
        -DANDROID_ABI=$ANDROID_ABI \
        -DANDROID_STL=c++_shared \
        -DCMAKE_TOOLCHAIN_FILE=~/android-ndk-r21/build/cmake/android.toolchain.cmake \
        -DOBOE_SOURCE="$OBOE_LOC" \
        -DALSOFT_REQUIRE_OBOE=ON \
        -DALSOFT_REQUIRE_OPENSL=ON \
        -DALSOFT_EMBED_HRTF_DATA=YES \
        ..
  # cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DANDROID_PLATFORM=21 -DANDROID_ABI=$ANDROID_ABI -DCMAKE_TOOLCHAIN_FILE=$ANDROID_SDK_ROOT/ndk-bundle/build/cmake/android.toolchain.cmake
  cmake --build . --clean-first
  cd ..
  cp build-$ANDROID_ABI/libopenal.so lib/$ANDROID_ABI/
}

mkdir -p lib

cmake_build arm64-v8a
cmake_build armeabi-v7a
cmake_build x86
cmake_build x86_64

