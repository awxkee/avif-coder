#!/bin/bash
#
# MIT License
#
# Copyright (c) 2023 Radzivon Bartoshyk
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
#

set -e
export NDK_PATH="/Users/radzivon/Library/Android/sdk/ndk/27.0.12077973"
export NDK=$NDK_PATH

destination_directory=x265_git
if [ ! -d "$destination_directory" ]; then
    git clone https://bitbucket.org/multicoreware/x265_git.git -b 3.6
else
    echo "Destination directory '$destination_directory' already exists. Cloning skipped."
fi

cd x265_git

#ABI_LIST="armeabi-v7a arm64-v8a x86 x86_64"

ABI_LIST="arm64-v8a"

for abi in ${ABI_LIST}; do
  rm -rf "build-${abi}"
  mkdir "build-${abi}"
  cd "build-${abi}"
 ARCH_OPTIONS=""
  case ${abi} in
      arm-v7a | arm-v7a-neon | armeabi-v7a)
         ARCH_OPTIONS="-DENABLE_ASSEMBLY=0 -DCROSS_COMPILE_ARM=1 -DCROSS_COMPILE_ARM64=0"
      ;;
      arm64-v8a)
         ARCH_OPTIONS="-DENABLE_ASSEMBLY=0 -DCROSS_COMPILE_ARM=0 -DCROSS_COMPILE_ARM64=0"
      ;;
      x86)
         ARCH_OPTIONS="-DENABLE_ASSEMBLY=0 -DCROSS_COMPILE_ARM=0"
      ;;
      x86_64)
         ARCH_OPTIONS="-DENABLE_ASSEMBLY=0 -DCROSS_COMPILE_ARM=0"
      ;;
  esac

  echo $ARCH_OPTIONS
  cmake ../source \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$NDK_PATH/build/cmake/android.toolchain.cmake -DANDROID_ABI=${abi} -DCMAKE_ANDROID_ARCH_ABI=${abi} \
    -DANDROID_NDK=${NDK_PATH} \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_ANDROID_STL_TYPE=c++_shared \
    -DCMAKE_SYSTEM_NAME=Android \
    -DCMAKE_THREAD_PREFER_PTHREAD=TRUE \
    -DENABLE_CLI=OFF \
    -DTHREADS_PREFER_PTHREAD_FLAG=TRUE \
    $ARCH_OPTIONS \
    -DBUILD_STATIC_LIBS=OFF \
    -DCMAKE_ASM_COMPILER=nasm
  ninja
  $NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip libx265.so
  cd ..
done

for abi in ${ABI_LIST}; do
  mkdir -p "../avif-coder/src/main/cpp/lib/${abi}"
  cp -r "build-${abi}/libx265.so" "../avif-coder/src/main/cpp/lib/${abi}/libx265.so"
  echo "build-${abi}/libx265.so was successfully copied to ../avif-coder/src/main/cpp/lib/${abi}/libx265.so!"
done