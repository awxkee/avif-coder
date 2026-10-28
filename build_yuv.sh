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

set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
REPO_DIR="$PWD"
LIBYUV_REVISION=4bc04198654971d9190475bbc9bbef8ae3b97aa7
NDK_PATH="${NDK_PATH:-${ANDROID_HOME:-$HOME/Library/Android/sdk}/ndk/28.2.13676358}"
LIBYUV_SOURCE_DIR="${LIBYUV_SOURCE_DIR:-$REPO_DIR/libyuv}"
if [[ ! -d "$LIBYUV_SOURCE_DIR" ]]; then
  git clone https://chromium.googlesource.com/libyuv/libyuv "$LIBYUV_SOURCE_DIR"
  git -C "$LIBYUV_SOURCE_DIR" checkout "$LIBYUV_REVISION"
fi
if [[ "$(git -C "$LIBYUV_SOURCE_DIR" rev-parse HEAD)" != "$LIBYUV_REVISION" ]]; then
  echo "Expected libyuv $LIBYUV_REVISION. Set LIBYUV_SOURCE_DIR to a fresh directory." >&2
  exit 1
fi
for abi in armeabi-v7a arm64-v8a x86 x86_64; do
  cmake -S "$LIBYUV_SOURCE_DIR" -B "$LIBYUV_SOURCE_DIR/build-$abi" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$NDK_PATH/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$abi" -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DLIBYUV_DISABLE_JPEG=ON
  cmake --build "$LIBYUV_SOURCE_DIR/build-$abi" --target yuv --parallel
  cp "$LIBYUV_SOURCE_DIR/build-$abi/libyuv.a" "$REPO_DIR/avif-coder/src/main/cpp/lib/$abi/libyuv.a"
done
cp -R "$LIBYUV_SOURCE_DIR/include/." "$REPO_DIR/avif-coder/src/main/cpp/libyuv/"
