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

export NDK=$NDK_PATH

destination_directory=SVT-AV1
if [ ! -d "$destination_directory" ]; then
    git clone https://gitlab.com/AOMediaCodec/SVT-AV1 -b v2.1.2
else
    echo "Destination directory '$destination_directory' already exists. Cloning skipped."
fi

cd $destination_directory

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
    -G "Ninja" \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_DOCS=0 \
    -DENABLE_EXAMPLES=0 \
    -DMINIMAL_BUILD=1 \
    -DENABLE_TESTDATA=0 \
    -DENABLE_TESTS=0 \
    -DENABLE_TOOLS=0 \
    -DBUILD_APPS=0 \
    -DBUILD_TESTING=0 \
    -DANDROID_ABI=${abi}

  ninja
  cp ../Bin/Release/libSvtAv1Enc.so libSvtAv1Enc.so
  $NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip libSvtAv1Enc.so
  cd ..
done
#
for abi in ${ABI_LIST}; do
  mkdir -p "../avif-coder/src/main/cpp/lib/${abi}"
  cp -r "build-${abi}/libSvtAv1Enc.so" "../avif-coder/src/main/cpp/lib/${abi}/libSvtAv1Enc.so"
  echo "build-${abi}/libSvtAv1Enc.so was successfully copied to ../avif-coder/src/main/cpp/lib/${abi}/libSvtAv1Enc.so!"
done