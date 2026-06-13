/*
 * Copyright (c) Radzivon Bartoshyk 2026/6. All rights reserved.
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

/*
 * Copyright (c) Radzivon Bartoshyk 2026/6. All rights reserved.
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

use crate::WeaverPreferredColorConfig;
use jni::Env;
use jni::sys::{JNIEnv, jobject};
use ndk_sys::{
    AndroidBitmap_getInfo, AndroidBitmap_lockPixels, AndroidBitmap_unlockPixels,
    AndroidBitmapFormat, AndroidBitmapInfo,
};
use std::fmt;

/// `ANDROID_BITMAP_FLAGS_IS_HARDWARE` — set when the bitmap is backed by an
/// `AHardwareBuffer` and therefore has no CPU-mappable pixels.
const FLAGS_IS_HARDWARE: u32 = 1 << 31;
/// `ANDROID_BITMAP_FLAGS_ALPHA_MASK`.
const FLAGS_ALPHA_MASK: u32 = 0x3;

// ── Describing what we read back ─────────────────────────────────────────────

/// The pixel format reported by `AndroidBitmap_getInfo`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum BitmapPixelFormat {
    /// 4 × u8, byte order R, G, B, A.
    Rgba8888,
    /// One packed little-endian u16, 5/6/5 bits.
    Rgb565,
    /// 4 × f16.
    RgbaF16,
    /// One packed little-endian u32, 10/10/10/2 bits.
    Rgba1010102,
    /// 1 × u8 (coverage only).
    A8,
}

impl BitmapPixelFormat {
    fn from_ndk(format: u32) -> Option<Self> {
        match format {
            f if f == AndroidBitmapFormat::ANDROID_BITMAP_FORMAT_RGBA_8888.0 => {
                Some(Self::Rgba8888)
            }
            f if f == AndroidBitmapFormat::ANDROID_BITMAP_FORMAT_RGB_565.0 => Some(Self::Rgb565),
            f if f == AndroidBitmapFormat::ANDROID_BITMAP_FORMAT_RGBA_F16.0 => Some(Self::RgbaF16),
            f if f == AndroidBitmapFormat::ANDROID_BITMAP_FORMAT_RGBA_1010102.0 => {
                Some(Self::Rgba1010102)
            }
            f if f == AndroidBitmapFormat::ANDROID_BITMAP_FORMAT_A_8.0 => Some(Self::A8),
            _ => None,
        }
    }

    /// Bytes occupied by a single pixel.
    pub fn bytes_per_pixel(self) -> usize {
        match self {
            Self::Rgba8888 | Self::Rgba1010102 => 4,
            Self::RgbaF16 => 8,
            Self::Rgb565 => 2,
            Self::A8 => 1,
        }
    }

    /// The native data type of the (possibly packed) pixel element.
    pub fn component_type(self) -> BitmapComponentType {
        match self {
            Self::Rgba8888 | Self::A8 => BitmapComponentType::U8,
            Self::RgbaF16 => BitmapComponentType::F16,
            Self::Rgb565 => BitmapComponentType::PackedU16,
            Self::Rgba1010102 => BitmapComponentType::PackedU32,
        }
    }

    /// Maps back onto the pipeline's `PreferredColorConfig`, where one exists.
    pub fn to_preferred_color(self) -> Option<WeaverPreferredColorConfig> {
        match self {
            Self::Rgba8888 => Some(WeaverPreferredColorConfig::Rgba8888),
            Self::Rgb565 => Some(WeaverPreferredColorConfig::Rgb565),
            Self::RgbaF16 => Some(WeaverPreferredColorConfig::RgbaF16),
            Self::Rgba1010102 => Some(WeaverPreferredColorConfig::Rgba1010102),
            Self::A8 => None,
        }
    }
}

/// The underlying numeric type one element of the pixel buffer should be read
/// as — this is the "data type" of the returned bytes.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum BitmapComponentType {
    /// Interpret bytes as `u8` (8-bit unsigned-normalized components).
    U8,
    /// Interpret each 2-byte pair as an IEEE-754 half-float.
    F16,
    /// Interpret each 2-byte pair as a packed little-endian `u16`.
    PackedU16,
    /// Interpret each 4-byte group as a packed little-endian `u32`.
    PackedU32,
}

/// How alpha is stored, from the bitmap info flags.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum BitmapAlpha {
    Premultiplied,
    Opaque,
    Unpremultiplied,
}

impl BitmapAlpha {
    fn from_flags(flags: u32) -> Self {
        match flags & FLAGS_ALPHA_MASK {
            1 => Self::Opaque,
            2 => Self::Unpremultiplied,
            _ => Self::Premultiplied, // 0
        }
    }
}

/// The pixel data extracted from a `Bitmap`, plus everything needed to interpret
/// it. `data` is tightly packed (row stride padding removed): exactly
/// `width * height * format.bytes_per_pixel()` bytes.
pub(crate) struct BitmapData {
    pub data: Vec<u8>,
    pub width: usize,
    pub height: usize,
    /// The pixel format reported by the bitmap.
    pub format: BitmapPixelFormat,
    /// How to read one element of `data`.
    #[allow(unused)]
    pub component_type: BitmapComponentType,
    /// Alpha storage mode.
    pub alpha: BitmapAlpha,
}

impl BitmapData {
    /// Raw bytes, tightly packed.
    #[inline]
    pub fn as_bytes(&self) -> &[u8] {
        &self.data
    }

    /// Bytes in one packed row.
    #[inline]
    pub fn row_bytes(&self) -> usize {
        self.width * self.format.bytes_per_pixel()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BitmapReadError {
    /// `AndroidBitmap_getInfo` returned a non-zero result code.
    GetInfo(i32),
    /// `AndroidBitmap_lockPixels` returned a non-zero result code.
    Lock(i32),
    /// Lock succeeded but handed back a null address.
    NullPixels,
    /// The bitmap uses a format we don't map (e.g. deprecated RGBA_4444).
    UnsupportedFormat(u32),
    /// A HARDWARE bitmap has no CPU pixels — copy it to a software config first
    /// (e.g. `Bitmap.copy(Bitmap.Config.ARGB_8888, false)`).
    HardwareBitmap,
    /// `width * height * bpp` overflowed `usize`.
    SizeOverflow,
}

impl fmt::Display for BitmapReadError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::GetInfo(c) => write!(f, "AndroidBitmap_getInfo failed (code {c})"),
            Self::Lock(c) => write!(f, "AndroidBitmap_lockPixels failed (code {c})"),
            Self::NullPixels => f.write_str("locked bitmap returned a null pixel address"),
            Self::UnsupportedFormat(v) => write!(f, "unsupported bitmap format {v}"),
            Self::HardwareBitmap => f.write_str(
                "HARDWARE bitmaps have no CPU-mappable pixels; copy to a software config first",
            ),
            Self::SizeOverflow => f.write_str("bitmap dimensions overflowed usize"),
        }
    }
}

impl std::error::Error for BitmapReadError {}

pub(crate) unsafe fn get_bitmap_data(
    env: &mut Env,
    bitmap: jobject,
) -> Result<BitmapData, BitmapReadError> {
    let mut info: AndroidBitmapInfo = unsafe { std::mem::zeroed() };
    let ret = unsafe { AndroidBitmap_getInfo(env.get_raw().cast(), bitmap.cast(), &mut info) };
    if ret != 0 {
        return Err(BitmapReadError::GetInfo(ret));
    }

    if info.flags & FLAGS_IS_HARDWARE != 0 {
        return Err(BitmapReadError::HardwareBitmap);
    }

    let format = BitmapPixelFormat::from_ndk(info.format as u32)
        .ok_or(BitmapReadError::UnsupportedFormat(info.format as u32))?;
    let width = info.width as usize;
    let height = info.height as usize;
    let stride = info.stride as usize;
    let alpha = BitmapAlpha::from_flags(info.flags);

    let row_bytes = width
        .checked_mul(format.bytes_per_pixel())
        .ok_or(BitmapReadError::SizeOverflow)?;
    let total = row_bytes
        .checked_mul(height)
        .ok_or(BitmapReadError::SizeOverflow)?;

    let mut data: Vec<u8> = Vec::with_capacity(total);

    let mut addr: *mut std::ffi::c_void = std::ptr::null_mut();
    let ret = unsafe { AndroidBitmap_lockPixels(env.get_raw().cast(), bitmap.cast(), &mut addr) };
    if ret != 0 {
        return Err(BitmapReadError::Lock(ret));
    }
    if addr.is_null() {
        unsafe { AndroidBitmap_unlockPixels(env.get_raw().cast(), bitmap.cast()) };
        return Err(BitmapReadError::NullPixels);
    }

    let components_size = match format {
        BitmapPixelFormat::Rgba8888 => 4 * size_of::<u8>(),
        BitmapPixelFormat::Rgb565 => 2 * size_of::<u8>(),
        BitmapPixelFormat::RgbaF16 => 4 * size_of::<u16>(),
        BitmapPixelFormat::Rgba1010102 => 4 * size_of::<u8>(),
        BitmapPixelFormat::A8 => size_of::<u8>(),
    };

    let src = addr as *const u8;
    for y in 0..height {
        // SAFETY: each row of `row_bytes` lives within the locked region; rows
        // are `stride` bytes apart. `data` has capacity for `total`, so
        // extend_from_slice never reallocates and never panics.
        let row = unsafe { std::slice::from_raw_parts(src.add(y * stride), row_bytes) };
        data.extend_from_slice(&row[..components_size * width]);
    }

    let _ = unsafe { AndroidBitmap_unlockPixels(env.get_raw().cast(), bitmap.cast()) };

    Ok(BitmapData {
        data,
        width,
        height,
        format,
        component_type: format.component_type(),
        alpha,
    })
}

pub unsafe fn is_hardware_bitmap(
    env: *mut JNIEnv,
    bitmap: jobject,
) -> Result<bool, BitmapReadError> {
    let mut info: AndroidBitmapInfo = unsafe { std::mem::zeroed() };
    let ret = unsafe { AndroidBitmap_getInfo(env.cast(), bitmap.cast(), &mut info) };
    if ret != 0 {
        return Err(BitmapReadError::GetInfo(ret));
    }
    Ok(info.flags & FLAGS_IS_HARDWARE != 0)
}
