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

destination_directory=aom
if [ ! -d "$destination_directory" ]; then
    git clone https://aomedia.googlesource.com/aom -b v3.10.0
else
    echo "Destination directory '$destination_directory' already exists. Cloning skipped."
fi

cd aom

ABI_LIST="armeabi-v7a arm64-v8a x86 x86_64"

for abi in ${ABI_LIST}; do
  rm -rf "build-${abi}"
  mkdir "build-${abi}"
  cd "build-${abi}"

  if [ "$abi" == "x86_64" ] || [ "$abi" == "x86" ]; then
        cmake .. \
          -G Ninja \
          -DCMAKE_TOOLCHAIN_FILE=$NDK_PATH/build/cmake/android.toolchain.cmake \
          -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
          -DANDROID_PLATFORM=android-24 \
          -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_SHARED_LIBS=ON \
          -DCMAKE_BUILD_TYPE=Release \
          -DENABLE_DOCS=0 \
          -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-z,max-page-size=16384" \
          -DAOM_TARGET_CPU=generic \
          -DENABLE_EXAMPLES=0 \
          -DENABLE_TESTDATA=0 \
          -DCONFIG_AV1_DECODER=OFF \
          -DCONFIG_MULTITHREAD=0 \
          -DENABLE_TESTS=0 \
          -DENABLE_TOOLS=0 \
          -DCONFIG_PIC=1 \
          -DCONFIG_AV1_DECODER=0 \
          -DANDROID_ABI=${abi} \
          -DCMAKE_ASM_NASM_COMPILER=/opt/homebrew/bin/nasm
  else
    cmake .. \
      -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=$NDK_PATH/build/cmake/android.toolchain.cmake \
      -DANDROID_PLATFORM=android-24 \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_DOCS=0 \
      -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-z,max-page-size=16384" \
      -DCONFIG_AV1_DECODER=OFF \
      -DENABLE_EXAMPLES=0 \
      -DENABLE_TESTDATA=0 \
      -DCONFIG_MULTITHREAD=1 \
      -DENABLE_TESTS=0 \
      -DENABLE_TOOLS=0 \
      -DCONFIG_PIC=1 \
      -DANDROID_ABI=${abi} \
      -DCMAKE_ASM_COMPILER=/opt/homebrew/bin/nasm
  fi
  ninja

  # shellcheck disable=SC2116
  current_folder=$(echo pwd)
  echo "libaom has built for arch ${abi} at path ${current_folder}"
  $NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip libaom.so
  cd ..
done

for abi in ${ABI_LIST}; do
#  mkdir -p "../avif-coder/src/main/cpp/lib/${abi}"
  cp -r "build-${abi}/libaom.so" "../avif-coder/src/main/cpp/lib/${abi}/libaom.so"
  echo "build-${abi}/libaom.so was successfully copied to ../avif-coder/src/main/cpp/lib/${abi}/libaom.so!"
done