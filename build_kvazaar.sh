#!/bin/bash
#
# MIT License
#
# Copyright (c) 2024 Radzivon Bartoshyk
# avif-coder [https://github.com/awxkee/avif-coder]
#
# Created by Radzivon Bartoshyk on 20/7/2024
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

destination_directory=kvazaar
if [ ! -d "$destination_directory" ]; then
  #  git clone https://github.com/ultravideo/kvazaar
  git clone --depth 1 https://github.com/ultravideo/kvazaar
else
    echo "Destination directory '$destination_directory' already exists. Cloning skipped."
fi

cd $destination_directory

ABI_LIST="armeabi-v7a arm64-v8a x86 x86_64"

for abi in ${ABI_LIST}; do
  rm -rf "build-${abi}"
  mkdir "build-${abi}"
  cd "build-${abi}"

  cmake .. \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$NDK_PATH/build/cmake/android.toolchain.cmake -DANDROID_ABI=${abi} -DCMAKE_ANDROID_ARCH_ABI=${abi} \
    -DANDROID_NDK=${NDK_PATH} \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_SYSTEM_NAME=Android \
    -DENABLE_CLI=OFF \
    -DBUILD_STATIC_LIBS=OFF
  ninja
  $NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip libkvazaar.so
  cd ..
done

for abi in ${ABI_LIST}; do
  mkdir -p "../avif-coder/src/main/cpp/lib/${abi}"
  cp -r "build-${abi}/libkvazaar.so" "../avif-coder/src/main/cpp/lib/${abi}/libkvazaar.so"
  echo "build-${abi}/libkvazaar.so was successfully copied to ../avif-coder/src/main/cpp/lib/${abi}/libkvazaar.so!"
done