#!/bin/bash
set -e

export NDK=$NDK_PATH

destination_directory=libheif
if [ ! -d "$destination_directory" ]; then
    git clone --depth 1 --branch v1.16.0 https://github.com/strukturag/libheif
else
    echo "Destination directory '$destination_directory' already exists. Cloning skipped."
fi

cd libheif

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
  cp -r ./../../libde265/build-${abi}/libde265/de265-version.h ../../libde265/libde265/de265-version.h
  cp -r ./../../x265/build-${abi}/x265_config.h ./../../x265/source/x265_config.h
  cp -r ./../../dav1d/build-${abi}/include/dav1d/version.h ./../../dav1d/include/dav1d/version.h
  cmake .. \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DWITH_EXAMPLES=0 \
    -DENABLE_PLUGIN_LOADING=0 \
    -DAOM_INCLUDE_DIR=../../aom \
    -DAOM_LIBRARY=../../aom/build-${abi}/libaom.a \
    -DX265_INCLUDE_DIR=../../x265/source \
    -DX265_LIBRARY=../../x265/build-${abi}/libx265.a \
    -DLIBDE265_LIBRARY=../../libde265/build-${abi}/libde265/libde265.a \
    -DLIBDE265_INCLUDE_DIR=../../libde265 \
    -DDAV1D_INCLUDE_DIR=../../dav1d/include \
    -DDAV1D_LIBRARY=../../dav1d/build-${abi}/src/libdav1d.a \
    -DANDROID_ABI=${abi}
  ninja
  cd ..
done

for abi in ${ABI_LIST}; do
  mkdir -p "../avif-coder/src/main/cpp/lib/${abi}"
  cp -r "build-${abi}/libheif/libheif.a" "../avif-coder/src/main/cpp/lib/${abi}/libheif.a"
  echo "build-${abi}/libheif/libheif.a was successfully copied to ../avif-coder/src/main/cpp/lib/${abi}/libheif.a!"
done