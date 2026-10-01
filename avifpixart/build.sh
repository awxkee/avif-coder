#!/usr/bin/env bash
#
# Copyright (c) Radzivon Bartoshyk. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
# 1.  Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# 2.  Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# 3.  Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

rustup +nightly target add x86_64-linux-android aarch64-linux-android armv7-linux-androideabi i686-linux-android

# Keep unwinding, but omit automatic panic backtraces, caller locations and detailed derived
# Debug output. Rebuilding std applies the same size reductions to its panic paths.
SIZE_RUSTFLAGS="-C link-arg=-Wl,-z,max-page-size=16384 -Z location-detail=none -Z fmt-debug=shallow"
STD_BUILD_FLAGS=(-Z build-std=std -Z build-std-features=panic-unwind)

RUSTFLAGS="$SIZE_RUSTFLAGS -C target-feature=+neon -C opt-level=3" cargo +nightly build "${STD_BUILD_FLAGS[@]}" --locked --target aarch64-linux-android --features rdm,i8mm,sve,logging --release --manifest-path Cargo.toml

RUSTFLAGS="$SIZE_RUSTFLAGS -C opt-level=z" cargo +nightly build "${STD_BUILD_FLAGS[@]}" --locked --no-default-features --target x86_64-linux-android --release --manifest-path Cargo.toml

RUSTFLAGS="$SIZE_RUSTFLAGS -C opt-level=z" cargo +nightly build "${STD_BUILD_FLAGS[@]}" --locked --no-default-features --target armv7-linux-androideabi --release --manifest-path Cargo.toml

RUSTFLAGS="$SIZE_RUSTFLAGS -C opt-level=z" cargo +nightly build "${STD_BUILD_FLAGS[@]}" --locked --target i686-linux-android --release --manifest-path Cargo.toml

cp -r target/aarch64-linux-android/release/libavifweaver.a ../avif-coder/src/main/cpp/lib/arm64-v8a/libavifweaver.a
cp -r target/x86_64-linux-android/release/libavifweaver.a ../avif-coder/src/main/cpp/lib/x86_64/libavifweaver.a
cp -r target/armv7-linux-androideabi/release/libavifweaver.a ../avif-coder/src/main/cpp/lib/armeabi-v7a/libavifweaver.a
cp -r target/i686-linux-android/release/libavifweaver.a ../avif-coder/src/main/cpp/lib/x86/libavifweaver.a
