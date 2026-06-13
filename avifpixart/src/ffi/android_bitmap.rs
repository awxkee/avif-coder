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
use crate::support::{PackedImageBuffer, PackedImageTransfer};

use crate::native_color_space::{NativeColorSpace, get_jni_color_space};
use jni::objects::{JObject, JValue};
use jni::strings::JNIString;
use jni::sys::{jboolean, jobject};
use jni::{Env, jni_sig, jni_str};

pub(crate) const MIN_OS_BITMAP_COLOR_SPACE: i32 = 34;

/// Maps a packed pixel format onto the matching `android.graphics.Bitmap$Config`
/// enum constant name.
fn bitmap_config_name(format: WeaverPreferredColorConfig) -> &'static str {
    match format {
        WeaverPreferredColorConfig::Rgb565 => "RGB_565",
        WeaverPreferredColorConfig::RgbaF16 => "RGBA_F16",
        WeaverPreferredColorConfig::Rgba1010102 => "RGBA_1010102",
        WeaverPreferredColorConfig::Hardware => "HARDWARE",
        WeaverPreferredColorConfig::Default | WeaverPreferredColorConfig::Rgba8888 => "ARGB_8888",
    }
}

/// RGB_565 has no alpha channel; every other software config we emit is RGBA.
fn format_has_alpha(format: WeaverPreferredColorConfig) -> bool {
    !matches!(format, WeaverPreferredColorConfig::Rgb565)
}

/// Resolves a `Bitmap$Config` enum constant by name.
fn bitmap_config<'local>(
    env: &mut Env<'local>,
    name: &str,
) -> Result<JObject<'local>, jni::errors::Error> {
    let cfg_cls = env.find_class(jni_str!("android/graphics/Bitmap$Config"))?;
    env.get_static_field(
        &cfg_cls,
        JNIString::from(name),
        jni_sig!("Landroid/graphics/Bitmap$Config;"),
    )?
    .l()
}
pub fn bitmap_from_transfer<'local>(
    env: &mut Env<'local>,
    transfer: PackedImageTransfer,
    color_space: Option<NativeColorSpace>,
) -> Result<JObject<'local>, jni::errors::Error> {
    match transfer {
        PackedImageTransfer::Image(buffer) => software_bitmap(env, &buffer, color_space),
        PackedImageTransfer::HardwareBuffer(hardware_buffer) => {
            wrap_hardware_buffer(env, hardware_buffer, color_space)
        }
    }
}

pub fn wrap_hardware_buffer<'local>(
    env: &mut Env<'local>,
    hardware_buffer: jobject,
    color_space: Option<NativeColorSpace>,
) -> Result<JObject<'local>, jni::errors::Error> {
    let bitmap_cls = env.find_class(jni_str!("android/graphics/Bitmap"))?;

    // SAFETY: `hardware_buffer` is a valid local ref returned by
    // `AHardwareBuffer_toHardwareBuffer`.
    let hb_obj = unsafe { JObject::from_raw(env, hardware_buffer) };

    let cs_obj = match color_space {
        Some(cs) => get_jni_color_space(env, cs)?,
        None => JObject::null(),
    };

    env.call_static_method(
        &bitmap_cls,
        JNIString::from("wrapHardwareBuffer"),
        jni_sig!(
            "(Landroid/hardware/HardwareBuffer;Landroid/graphics/ColorSpace;)Landroid/graphics/Bitmap;"
        ),
        &[JValue::Object(&hb_obj), JValue::Object(&cs_obj)],
    )?
        .l()
}

pub fn software_bitmap<'local>(
    env: &mut Env<'local>,
    buffer: &PackedImageBuffer,
    color_space: Option<NativeColorSpace>,
) -> Result<JObject<'local>, jni::errors::Error> {
    software_bitmap_with_alpha(env, buffer, format_has_alpha(buffer.format), color_space)
}

/// Like [`software_bitmap`] but with an explicit `has_alpha` flag, which controls
/// premultiplication when a `color_space` is attached.
pub fn software_bitmap_with_alpha<'local>(
    env: &mut Env<'local>,
    buffer: &PackedImageBuffer,
    has_alpha: bool,
    color_space: Option<NativeColorSpace>,
) -> Result<JObject<'local>, jni::errors::Error> {
    let config = bitmap_config(env, bitmap_config_name(buffer.format))?;
    let bitmap_cls = env.find_class(jni_str!("android/graphics/Bitmap"))?;

    let width = buffer.width as i32;
    let height = buffer.height as i32;

    let bitmap = match color_space {
        // createBitmap(int, int, Config, boolean, ColorSpace) — API 29+.
        Some(cs) => {
            let cs_obj = get_jni_color_space(env, cs)?;
            env.call_static_method(
                &bitmap_cls,
                JNIString::from("createBitmap"),
                jni_sig!(
                    "(IILandroid/graphics/Bitmap$Config;ZLandroid/graphics/ColorSpace;)Landroid/graphics/Bitmap;"
                ),
                &[
                    JValue::Int(width),
                    JValue::Int(height),
                    JValue::Object(&config),
                    JValue::Bool(has_alpha as jboolean),
                    JValue::Object(&cs_obj),
                ],
            )?
                .l()?
        }
        // createBitmap(int, int, Config) — defaults to sRGB.
        None => env
            .call_static_method(
                &bitmap_cls,
                JNIString::from("createBitmap"),
                jni_sig!("(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;"),
                &[
                    JValue::Int(width),
                    JValue::Int(height),
                    JValue::Object(&config),
                ],
            )?
            .l()?,
    };

    copy_pixels_into(env, &bitmap, &buffer.data)?;
    Ok(bitmap)
}

/// Copies raw pixel bytes into an already-created `Bitmap` via a direct
/// `ByteBuffer` + `Bitmap.copyPixelsFromBuffer`.
fn copy_pixels_into(
    env: &mut Env<'_>,
    bitmap: &JObject<'_>,
    data: &[u8],
) -> Result<(), jni::errors::Error> {
    // SAFETY: the direct ByteBuffer aliases `data` only for the duration of the
    // synchronous `copyPixelsFromBuffer` call below; we never expose it
    // afterward, and `data` outlives this function. `copyPixelsFromBuffer`
    // only reads, so the const→mut cast is not used to mutate.
    let byte_buffer = unsafe { env.new_direct_byte_buffer(data.as_ptr() as *mut u8, data.len())? };

    env.call_method(
        bitmap,
        JNIString::from("copyPixelsFromBuffer"),
        jni_sig!("(Ljava/nio/Buffer;)V"),
        &[JValue::Object(&byte_buffer)],
    )?;

    Ok(())
}

/// Whether `ColorSpace`-aware bitmap construction is available on this device.
/// Callers should fall back to the no-colorspace overloads below this level.
pub fn supports_bitmap_color_space(api_level: i32) -> bool {
    api_level >= MIN_OS_BITMAP_COLOR_SPACE
}
