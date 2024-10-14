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

set -e
rustup default stable

rustup +stable target add x86_64-linux-android aarch64-linux-android armv7-linux-androideabi i686-linux-android

RUSTFLAGS="-C target-feature=+neon -C opt-level=2 -C strip=symbols" cargo +stable build --target aarch64-linux-android --release --manifest-path Cargo.toml

RUSTFLAGS="-C opt-level=z -C strip=symbols" cargo +stable build --target x86_64-linux-android --release --manifest-path Cargo.toml

RUSTFLAGS="-C opt-level=z -C strip=symbols" cargo +stable build --target armv7-linux-androideabi --release --manifest-path Cargo.toml

RUSTFLAGS="-C opt-level=z -C strip=symbols" cargo +stable build --target i686-linux-android --release --manifest-path Cargo.toml


cp -r target/aarch64-linux-android/release/libavifweaver.so ../avif-coder/src/main/cpp/lib/arm64-v8a/libavifweaver.so
cp -r target/x86_64-linux-android/release/libavifweaver.so ../avif-coder/src/main/cpp/lib/x86_64/libavifweaver.so
cp -r target/armv7-linux-androideabi/release/libavifweaver.so ../avif-coder/src/main/cpp/lib/armeabi-v7a/libavifweaver.so
cp -r target/i686-linux-android/release/libavifweaver.so ../avif-coder/src/main/cpp/lib/x86/libavifweaver.so