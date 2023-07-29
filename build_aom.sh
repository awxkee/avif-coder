#!/bin/bash
set -e

destination_directory=aom
if [ ! -d "$destination_directory" ]; then
    git clone https://aomedia.googlesource.com/aom
else
    echo "Destination directory '$destination_directory' already exists. Cloning skipped."
fi

cd aom

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
    -DCMAKE_TOOLCHAIN_FILE=$NDK_PATH/build/cmake/android.toolchain.cmake \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_DOCS=0 \
    -DENABLE_EXAMPLES=0 \
    -DENABLE_TESTDATA=0 \
    -DENABLE_TESTS=0 \
    -DENABLE_TOOLS=0 \
    -DANDROID_ABI=${abi}
  ninja

  # shellcheck disable=SC2116
  current_folder=$(echo pwd)
  echo "libaom has built for arch ${abi} at path ${current_folder}"

  cd ..
done

for abi in ${ABI_LIST}; do
  mkdir -p "../avif-coder/src/main/cpp/lib/${abi}"
  cp -r "build-${abi}/libaom.a" "../avif-coder/src/main/cpp/lib/${abi}/libaom.a"
  echo "build-${abi}/libaom.a was successfully copied to ../avif-coder/src/main/cpp/lib/${abi}/libaom.a!"
done