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
DAV1D_VERSION=1.5.3
NDK_PATH="${NDK_PATH:-${ANDROID_HOME:-$HOME/Library/Android/sdk}/ndk/28.2.13676358}"
export NDK_PATH
DAV1D_SOURCE_DIR="${DAV1D_SOURCE_DIR:-$REPO_DIR/dav1d}"
if [[ ! -d "$DAV1D_SOURCE_DIR" ]]; then
  git clone --depth 1 --branch "$DAV1D_VERSION" https://code.videolan.org/videolan/dav1d "$DAV1D_SOURCE_DIR"
fi
if [[ "$(git -C "$DAV1D_SOURCE_DIR" describe --tags --exact-match)" != "$DAV1D_VERSION" ]]; then
  echo "Expected dav1d $DAV1D_VERSION. Set DAV1D_SOURCE_DIR to a fresh directory." >&2
  exit 1
fi
cd "$DAV1D_SOURCE_DIR"
for arch in arm arm64 x86 x86_64; do
  bash "$REPO_DIR/conf/dav1d_conf_build.sh" -a "$arch"
done
for abi in armeabi-v7a arm64-v8a x86 x86_64; do
  cp "build-$abi/src/libdav1d.so" "$REPO_DIR/avif-coder/src/main/cpp/lib/$abi/libdav1d.so"
done
cp include/dav1d/*.h "$REPO_DIR/avif-coder/src/main/cpp/avif/dav1d/"
