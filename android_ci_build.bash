#!/bin/bash
set -e

cmake_build () {
  ANDROID_ABI=$1
  mkdir -p build-$ANDROID_ABI
  mkdir -p lib/$ANDROID_ABI
  cd build-$ANDROID_ABI
  cmake $GITHUB_WORKSPACE -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DANDROID_PLATFORM=21 -DANDROID_ABI=$ANDROID_ABI -DCMAKE_TOOLCHAIN_FILE=$ANDROID_SDK_ROOT/ndk-bundle/build/cmake/android.toolchain.cmake
  cmake -v --build . --clean-first
  cd ..
  cp build-$ANDROID_ABI/libopenal.so lib/$ANDROID_ABI/
}

# Hack Android NDK to get 64-bit libs linked
# libdir=$ANDROID_SDK_ROOT/ndk-bundle/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib
# mv $libdir/arm-linux-androideabi/16 $libdir/arm-linux-androideabi/16z
# mv $libdir/i686-linux-android/16 $libdir/i686-linux-android/16z
# cp -R $libdir/aarch64-linux-android/21 $libdir/aarch64-linux-android/16
# cp -R $libdir/arm-linux-androideabi/21 $libdir/arm-linux-androideabi/16
# cp -R $libdir/i686-linux-android/21 $libdir/i686-linux-android/16
# cp -R $libdir/x86_64-linux-android/21 $libdir/x86_64-linux-android/16

# libdir=$ANDROID_SDK_ROOT/ndk-bundle/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include
# mv $libdir/arm-linux-androideabi/16 $libdir/arm-linux-androideabi/16z
# mv $libdir/i686-linux-android/16 $libdir/i686-linux-android/16z
# cp -R $libdir/aarch64-linux-android/21 $libdir/aarch64-linux-android/16
# cp -R $libdir/arm-linux-androideabi/21 $libdir/arm-linux-androideabi/16
# cp -R $libdir/i686-linux-android/21 $libdir/i686-linux-android/16
# cp -R $libdir/x86_64-linux-android/21 $libdir/x86_64-linux-android/16

# /aarch64-linux-android/21/

mkdir -p lib

cmake_build arm64-v8a
cmake_build armeabi-v7a
cmake_build x86
cmake_build x86_64

