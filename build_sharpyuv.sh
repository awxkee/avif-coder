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

export NDK_PATH="/Users/radzivon/Library/Android/sdk/ndk/28.0.12674087"

export NDK=$NDK_PATH

destination_directory=libwebp
if [ ! -d "$destination_directory" ]; then
    git clone https://github.com/webmproject/libwebp
else
    echo "Destination directory '$destination_directory' already exists. Cloning skipped."
fi

cd libwebp

if [ -z "$INCLUDE_X86" ]; then
  ABI_LIST="armeabi-v7a arm64-v8a x86_64"
  echo "X86 won't be included into a build"
else
  ABI_LIST="armeabi-v7a arm64-v8a x86 x86_64"
fi

for abi in ${ABI_LIST}; do
  rm -rf "build-${abi}"
  mkdir "build-${abi}"
  cd "build-${abi}"

  echo $ARCH_OPTIONS

  cmake .. \
    -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=${abi} -DCMAKE_ANDROID_ARCH_ABI=${abi} \
    -DANDROID_NDK=${NDK} \
    -DANDROID_PLATFORM=android-21 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_SYSTEM_NAME=Generic \
    -DCMAKE_ANDROID_STL_TYPE=c++_shared \
    -DCMAKE_SYSTEM_NAME=Android \
    -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-z,max-page-size=16384" \
    -DCMAKE_THREAD_PREFER_PTHREAD=TRUE \
    -DTHREADS_PREFER_PTHREAD_FLAG=TRUE \
    -DBUILD_STATIC_LIBS=ON

  cmake --build .
  cd ..
done
#
for abi in ${ABI_LIST}; do
  mkdir -p "../avif-coder/src/main/cpp/lib/${abi}"
  cp -r "build-${abi}/libsharpyuv.a" "../avif-coder/src/main/cpp/lib/${abi}/libsharpyuv.a"
  echo "build-${abi}/libsharpyuv.a was successfully copied to ../avif-coder/src/main/cpp/lib/${abi}/libsharpyuv.a!"
done