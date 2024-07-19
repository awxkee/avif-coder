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
destination_directory=dav1d
if [ ! -d "$destination_directory" ]; then
    git clone https://code.videolan.org/videolan/dav1d -b 1.4.3
else
    echo "Destination directory '$destination_directory' already exists. Cloning skipped."
fi

cp -r ./conf/dav1d_conf_build.sh ./dav1d/build.sh

cd dav1d

ABI_LIST="arm arm64 x86 x86_64"
ARMEABI_LIST="armeabi-v7a arm64-v8a x86 x86_64"

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
  cp -r "build-${abi}/src/libdav1d.so" "../avif-coder/src/main/cpp/lib/${abi}/libdav1d.so"
  echo "build-${abi}/src/libdav1d.so was successfully copied to ../avif-coder/src/main/cpp/lib/${abi}/libdav1d.so!"
done

