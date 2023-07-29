#!/bin/bash
set -e

export NDK=$NDK_PATH

destination_directory=libde265
if [ ! -d "$destination_directory" ]; then
    git clone https://github.com/strukturag/libde265
else
    echo "Destination directory '$destination_directory' already exists. Cloning skipped."
fi

cd libde265

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
  cmake .. \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_DOCS=0 \
    -DENABLE_EXAMPLES=0 \
    -DENABLE_TESTDATA=0 \
    -DENABLE_TESTS=0 \
    -DENABLE_TOOLS=0 \
    -DDISABLE_SSE=1 \
    -DANDROID_ABI=${abi}
  ninja
  cd ..
done

for abi in ${ABI_LIST}; do
  mkdir -p "../avif-coder/src/main/cpp/lib/${abi}"
  cp -r "build-${abi}/libde265/libde265.a" "../avif-coder/src/main/cpp/lib/${abi}/libde265.a"
  echo "build-${abi}/libde265/libde265.a was successfully copied to ../avif-coder/src/main/cpp/lib/${abi}/libde265.a!"
done