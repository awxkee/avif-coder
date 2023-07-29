#!/bin/bash
set -e

destination_directory=dav1d
if [ ! -d "$destination_directory" ]; then
    git clone https://code.videolan.org/videolan/dav1d
else
    echo "Destination directory '$destination_directory' already exists. Cloning skipped."
fi

cp -r ./conf/dav1d_conf_build.sh ./dav1d/build.sh

cd dav1d

if [ -z "$INCLUDE_X86" ]; then
  ABI_LIST="arm arm64 x86_64"
  ARMEABI_LIST="armeabi-v7a arm64-v8a x86_64"
  echo "X86 won't be included into a build"
else
  ABI_LIST="arm arm64 x86 x86_64"
  ARMEABI_LIST="armeabi-v7a arm64-v8a x86 x86_64"
fi

for ARCH in ${ABI_LIST}; do
  case "${ARCH}" in
    'arm')
    ABI=armeabi-v7a ;;
    'arm64')
        ABI=arm64-v8a ;;
    *)
        ABI=${ARCH} ;;
    esac
  if [[ ! -d dist-${ABI} ]]; then
    export LOCAL_PATH=../$pwd
    ( . build.sh -a ${ARCH} )
    # shellcheck disable=SC2116
    current_folder=$(echo pwd)
    echo "libdav1d has built for arch ${abi} at path ${current_folder}/$ARCH"
  fi
done

for abi in ${ARMEABI_LIST}; do
  mkdir -p "../avif-coder/src/main/cpp/lib/${abi}"
  cp -r "build-${abi}/src/libdav1d.a" "../avif-coder/src/main/cpp/lib/${abi}/libdav1d.a"
  echo "build-${abi}/src/libdav1d.a was successfully copied to ../avif-coder/src/main/cpp/lib/${abi}/libdav1d.a!"
done

