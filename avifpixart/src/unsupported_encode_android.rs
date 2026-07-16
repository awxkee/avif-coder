/*
 * Copyright (c) Radzivon Bartoshyk. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1.  Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2.  Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3.  Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

use crate::support::{init_logging, throw_runtime_exception_raw};
use jni::sys::{jbyteArray, jobject};
use std::ptr::null_mut;

const SUPPORTED_ENCODING_TARGETS: &str =
    "aarch64-linux-android and armv7-linux-androideabi";

#[repr(C)]
#[derive(Debug, Clone)]
pub enum AvEncodingSpeed {
    Slow,
    Medium,
    Fast,
}

#[inline]
unsafe fn unsupported_encoding(env: *mut jni::sys::JNIEnv, codec: &str) -> jbyteArray {
    init_logging();
    let message = format!(
        "{codec} encoding is not supported on target architecture '{}'. Supported targets: {SUPPORTED_ENCODING_TARGETS}",
        std::env::consts::ARCH,
    );
    unsafe { throw_runtime_exception_raw(env, message) };
    null_mut()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn encode_avif_av1_file(
    env: *mut jni::sys::JNIEnv,
    _image: jobject,
    _exif: jobject,
    _color_space: i32,
    _quality: i32,
    _lossless: bool,
    _chroma_subsampling_code: i32,
    _speed: AvEncodingSpeed,
) -> jbyteArray {
    unsafe { unsupported_encoding(env, "AV1/AVIF") }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn encode_avif_av2_file(
    env: *mut jni::sys::JNIEnv,
    _image: jobject,
    _exif: jobject,
    _color_space: i32,
    _quality: i32,
    _lossless: bool,
    _chroma_subsampling_code: i32,
    _speed: AvEncodingSpeed,
) -> jbyteArray {
    unsafe { unsupported_encoding(env, "AV2/AVIF") }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn encode_heic_file(
    env: *mut jni::sys::JNIEnv,
    _image: jobject,
    _exif: jobject,
    _color_space: i32,
    _quality: i32,
    _chroma_subsampling_code: i32,
    _lossless: bool,
) -> jbyteArray {
    unsafe { unsupported_encoding(env, "HEIC") }
}
