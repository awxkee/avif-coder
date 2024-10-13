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

destination_directory=libheif
if [ ! -d "$destination_directory" ]; then
   # git clone --depth 1 --branch v1.16.0 https://github.com/strukturag/libheif -b v1.18.0
     git clone --depth 1 https://github.com/strukturag/libheif
else
    echo "Destination directory '$destination_directory' already exists. Cloning skipped."
fi

cd libheif

ABI_LIST="armeabi-v7a arm64-v8a x86 x86_64"

for abi in ${ABI_LIST}; do
  rm -rf "build-${abi}"
  mkdir "build-${abi}"
  cd "build-${abi}"
  cp -r ./../../libde265/build-${abi}/libde265/de265-version.h ../../libde265/libde265/de265-version.h
  cp -r ./../../x265_git/build-${abi}/x265_config.h ./../../x265_git/source/x265_config.h
  cp -r ./../../dav1d/build-${abi}/include/dav1d/version.h ./../../dav1d/include/dav1d/version.h
#  mkdir -p ./../../SVT-AV1/svt-av1
#  cp -r ./../../SVT-AV1/Source/API/* ./../../SVT-AV1/svt-av1
  cmake .. \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DWITH_EXAMPLES=0 \
    -DENABLE_PLUGIN_LOADING=0 \
    -DWITH_AOM=OFF \
    -DWITH_DAV1D=OFF \
    -DAOM_DECODER=OFF \
    -DAOM_ENCODER=OFF \
    -DWITH_AOM_DECODER=OFF \
    -DWITH_KVAZAAR=ON \
    -DWITH_X265=ON \
    -DX265_INCLUDE_DIR=./../../x265_git/source \
    -DX265_LIBRARY=./../../x265_git/build-${abi}/libx265.so \
    -DLIBDE265_LIBRARY=./../../libde265/build-${abi}/libde265/libde265.so \
    -DLIBDE265_INCLUDE_DIR=../../libde265 \
    -DLIBSHARPYUV_INCLUDE_DIR=../../libwebp \
    -DLIBSHARPYUV_LIBRARY=../../libwebp/build-${abi}/libsharpyuv.a \
    -DENABLE_MULTITHREADING_SUPPORT=TRUE \
    -DENABLE_PARALLEL_TILE_DECODING=TRUE \
    -DBUILD_TESTING=OFF \
    -DANDROID_ABI=${abi}
  ninja
  ${NDK_PATH}/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip libheif/libheif.so
  cd ..
done


for abi in ${ABI_LIST}; do
  mkdir -p "../avif-coder/src/main/cpp/lib/${abi}"
  cp -r "build-${abi}/libheif/libheif.so" "../avif-coder/src/main/cpp/lib/${abi}/libheif.so"
  echo "build-${abi}/libheif/libheif.so was successfully copied to ../avif-coder/src/main/cpp/lib/${abi}/libheif.so!"
done