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

use crate::heic_decode_android::HeicInfo;
use crate::support::{init_logging, throw_runtime_exception_raw};
use crate::{WeaveScaleMode, WeaverPreferredColorConfig};
use jni::sys::jobject;
use std::ptr::null_mut;

const SUPPORTED_AV2_DECODING_TARGETS: &str =
    "aarch64-linux-android and armv7-linux-androideabi";

#[unsafe(no_mangle)]
pub unsafe extern "C" fn decode_av2_file(
    env: *mut jni::sys::JNIEnv,
    _data: *const u8,
    _length: usize,
    _scaled_width: i32,
    _scaled_height: i32,
    _scale_mode: WeaveScaleMode,
    _preferred_color_config: WeaverPreferredColorConfig,
) -> jobject {
    init_logging();
    let message = format!(
        "AV2 decoding is not supported on target architecture '{}'. Supported targets: {SUPPORTED_AV2_DECODING_TARGETS}",
        std::env::consts::ARCH,
    );
    unsafe { throw_runtime_exception_raw(env, message) };
    null_mut()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn read_av2_file_info(_data: *const u8, _length: usize) -> HeicInfo {
    // This ABI has no error/exception parameter, so report the image as unsupported.
    HeicInfo::not_a_heic()
}
