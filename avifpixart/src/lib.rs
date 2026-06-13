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

#![allow(clippy::missing_safety_doc)]
#![feature(f16)]

mod av1_encode_android;
mod box_walker;
mod cvt;
mod ffi;
mod heic_decode;
#[cfg(target_os = "android")]
mod heic_decode_android;
mod heic_encode_android;
mod icc;
mod native_color_space;
mod orientation;
mod rgb_to_yuv;
mod scaling;
mod support;
mod tonemapper;
mod yuv_ffi;

use std::fmt::Debug;
use std::slice;

pub use av1_encode_android::encode_avif_av1_file;
pub use box_walker::{is_avif_image, is_heic_image};
pub use cvt::{
    weave_cvt_rgba8_to_ar30, weave_cvt_rgba8_to_rgba_f16, weave_cvt_rgba16_to_ar30,
    weave_cvt_rgba16_to_rgba_f16, weave_premultiply_rgba_f16,
};
pub use heic_decode_android::{HeicInfo, decode_heic_file, read_heic_file_info};
pub use heic_encode_android::encode_heic_file;
pub use rgb_to_yuv::{weave_rgba8_to_y08, weave_rgba8_to_yuv8};
pub use scaling::{
    ScalingFunction, ScalingResult, ScalingResultU16, WeaveScaleMode, weave_scale_f16,
    weave_scale_u8, weave_scale_u16, weave_scaling_result_free, weave_scaling_result16_free,
};
pub use tonemapper::{FfiTrc, ToneMapping, apply_tone_mapping_rgba8, apply_tone_mapping_rgba16};
pub use yuv_ffi::*;

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Ord, PartialOrd, Eq)]
pub enum WeaverPreferredColorConfig {
    Default,
    Rgba8888 = 2,
    RgbaF16 = 3,
    Rgb565 = 4,
    Rgba1010102 = 5,
    Hardware = 6,
}

pub(crate) fn check_image_size_overflow(width: u64, height: u64, chan: u64, t_size: isize) -> bool {
    let Ok(w) = isize::try_from(width) else {
        return true;
    };
    let Ok(h) = isize::try_from(height) else {
        return true;
    };
    let Ok(n) = isize::try_from(chan) else {
        return true;
    };
    let Some(stride) = w.checked_mul(n) else {
        return true;
    };

    // stride * (height - 1) + width * N
    let Some(h_minus_1) = h.checked_sub(1) else {
        return true;
    };
    let Some(lhs) = stride.checked_mul(h_minus_1) else {
        return true;
    };
    lhs.checked_add(stride)
        .and_then(|x| x.checked_mul(t_size))
        .is_none()
}
