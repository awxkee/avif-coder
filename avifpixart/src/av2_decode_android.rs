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
use crate::av2_decode::{PackedAv2, decode_packed_av2};
use crate::cvt::{
    pack_8_to_565, pack_8_to_ar30, pack_8_to_f16, pack_10_or_12_to_565, pack_10_or_12_to_ar30,
    pack_10_or_12_to_f16,
};
use crate::ffi::{MIN_HARDWARE_BUFFER_OS, load_hardware_buffer_api};
use crate::ffi::{MIN_OS_BITMAP_COLOR_SPACE, software_bitmap, wrap_hardware_buffer};
use crate::heic_decode::WeaverError;
use crate::native_color_space::NativeColorSpace;
use crate::scaling::{internal_scale_u8, internal_scale_u16};
use crate::support::{
    MIN_OS_AR30, MIN_OS_F16, PackedImageBuffer, PackedImageTransfer, android_os_version, dbg_log,
    init_logging, try_vec,
};
use crate::{HeicInfo, WeaveScaleMode, WeaverPreferredColorConfig, is_av2_image};
use hpvcd::BitDepth;
use jni::objects::JObject;
use jni::strings::JNIString;
use jni::sys::jobject;
use jni::{Env, EnvUnowned, Outcome, jni_str};
use moxcms::{ColorProfile, Layout, TransformOptions};
use ndk_sys::{AHardwareBuffer_Desc, AHardwareBuffer_Format, AHardwareBuffer_UsageFlags, ARect};
use std::ptr::null_mut;
use std::slice;
use tealdust::{AvifDecoder, ColorInfo};

fn resolve_cicp_profile(colors: ColorInfo) -> Option<ColorProfile> {
    match colors.color_primaries {
        tealdust::ColorPrimaries::Bt709 | tealdust::ColorPrimaries::Bt601 => {
            if colors.transfer_characteristics == tealdust::TransferCharacteristics::Srgb {
                Some(ColorProfile::new_srgb())
            } else {
                None
            }
        }
        tealdust::ColorPrimaries::Bt2020 => {
            if colors.transfer_characteristics == tealdust::TransferCharacteristics::Srgb {
                Some(ColorProfile::new_bt2020())
            } else {
                None
            }
        }
        tealdust::ColorPrimaries::Smpte432 => {
            // Display P3, P3 primaries on a D65 white point with sRGB transfer
            if colors.transfer_characteristics == tealdust::TransferCharacteristics::Srgb {
                Some(ColorProfile::new_display_p3())
            } else {
                None
            }
        }
        tealdust::ColorPrimaries::Smpte431 => {
            // DCI-P3, P3 primaries on DCI white point with 2.6γ (SMPTE ST 428-1)
            if colors.transfer_characteristics == tealdust::TransferCharacteristics::Smpte428 {
                Some(ColorProfile::new_dci_p3())
            } else {
                None
            }
        }
        tealdust::ColorPrimaries::Unknown
        | tealdust::ColorPrimaries::Bt470M
        | tealdust::ColorPrimaries::Bt470Bg
        | tealdust::ColorPrimaries::Smpte240
        | tealdust::ColorPrimaries::Film
        | tealdust::ColorPrimaries::Xyz
        | tealdust::ColorPrimaries::Ebu3213
        | tealdust::ColorPrimaries::Reserved => None,
    }
}

fn resolve_native_profile(colors: ColorInfo) -> Option<NativeColorSpace> {
    match colors.color_primaries {
        tealdust::ColorPrimaries::Bt709 | tealdust::ColorPrimaries::Bt601 => {
            match colors.transfer_characteristics {
                tealdust::TransferCharacteristics::Srgb => Some(NativeColorSpace::DefaultSrgb),
                tealdust::TransferCharacteristics::Linear => Some(NativeColorSpace::LinearSrgb),
                tealdust::TransferCharacteristics::Bt709 => Some(NativeColorSpace::Bt709),
                _ => None,
            }
        }
        tealdust::ColorPrimaries::Bt2020 => match colors.transfer_characteristics {
            tealdust::TransferCharacteristics::Smpte2084 => Some(NativeColorSpace::Pq2100),
            tealdust::TransferCharacteristics::Hlg => Some(NativeColorSpace::Hlg2100),
            _ => None,
        },
        tealdust::ColorPrimaries::Smpte432 => match colors.transfer_characteristics {
            // Display P3 — P3 primaries, D65 white point, sRGB transfer
            tealdust::TransferCharacteristics::Srgb => Some(NativeColorSpace::DisplayP3),
            _ => None,
        },
        tealdust::ColorPrimaries::Smpte431 => match colors.transfer_characteristics {
            // DCI-P3 — P3 primaries, DCI white point, 2.6γ (SMPTE ST 428-1)
            tealdust::TransferCharacteristics::Smpte428 => Some(NativeColorSpace::DciP3),
            _ => None,
        },
        tealdust::ColorPrimaries::Unknown
        | tealdust::ColorPrimaries::Bt470M
        | tealdust::ColorPrimaries::Bt470Bg
        | tealdust::ColorPrimaries::Smpte240
        | tealdust::ColorPrimaries::Film
        | tealdust::ColorPrimaries::Xyz
        | tealdust::ColorPrimaries::Ebu3213
        | tealdust::ColorPrimaries::Reserved => None,
    }
}

fn make_8bit_transfer(
    env: &mut Env,
    data: &[u8],
    width: usize,
    height: usize,
    preferred_color: WeaverPreferredColorConfig,
) -> Result<PackedImageTransfer, anyhow::Error> {
    match preferred_color {
        WeaverPreferredColorConfig::Default | WeaverPreferredColorConfig::Rgba8888 => {
            Ok(PackedImageTransfer::Image(PackedImageBuffer {
                data: data.to_vec(),
                width,
                height,
                format: WeaverPreferredColorConfig::Rgba8888,
            }))
        }
        WeaverPreferredColorConfig::RgbaF16 => {
            if android_os_version() >= MIN_OS_F16 {
                Ok(PackedImageTransfer::Image(PackedImageBuffer {
                    data: pack_8_to_f16(data, width, height)?,
                    width,
                    height,
                    format: WeaverPreferredColorConfig::RgbaF16,
                }))
            } else {
                Ok(PackedImageTransfer::Image(PackedImageBuffer {
                    data: data.to_vec(),
                    width,
                    height,
                    format: WeaverPreferredColorConfig::RgbaF16,
                }))
            }
        }
        WeaverPreferredColorConfig::Rgb565 => Ok(PackedImageTransfer::Image(PackedImageBuffer {
            data: pack_8_to_565(data, width, height)?,
            width,
            height,
            format: WeaverPreferredColorConfig::Rgb565,
        })),
        WeaverPreferredColorConfig::Rgba1010102 => {
            if android_os_version() >= MIN_OS_AR30 {
                Ok(PackedImageTransfer::Image(PackedImageBuffer {
                    data: pack_8_to_ar30(data, width, height)?,
                    width,
                    height,
                    format: WeaverPreferredColorConfig::Rgba1010102,
                }))
            } else {
                Ok(PackedImageTransfer::Image(PackedImageBuffer {
                    data: data.to_vec(),
                    width,
                    height,
                    format: WeaverPreferredColorConfig::RgbaF16,
                }))
            }
        }
        WeaverPreferredColorConfig::Hardware => {
            if android_os_version() >= MIN_HARDWARE_BUFFER_OS
                && let Some(hw_apis) = load_hardware_buffer_api()
            {
                let usage = AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN.0
                    | AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN.0
                    | AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE.0
                    | AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT.0;
                let descriptor = AHardwareBuffer_Desc {
                    width: width as u32,
                    height: height as u32,
                    layers: 1,
                    format: AHardwareBuffer_Format::AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM.0,
                    usage,
                    stride: 0,
                    rfu0: 0,
                    rfu1: 0,
                };
                if !hw_apis.is_supported(&descriptor) {
                    Ok(PackedImageTransfer::Image(PackedImageBuffer {
                        data: data.to_vec(),
                        width,
                        height,
                        format: WeaverPreferredColorConfig::Rgba8888,
                    }))
                } else {
                    let mut owned = hw_apis.allocate_owned(&descriptor).map_err(|x| {
                        anyhow::anyhow!("Allocation hardware buffer failed with an error {x}")
                    })?;
                    let usage_flags =
                        AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN.0
                            | AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN.0;
                    let a_rect = ARect {
                        left: 0,
                        top: 0,
                        right: width as i32,
                        bottom: height as i32,
                    };
                    {
                        let mut lock = owned.lock(usage_flags, -1, Some(&a_rect)).map_err(|x| {
                            anyhow::anyhow!("Locking hardware buffer failed with an error {x}")
                        })?;
                        let total_length = width * height * 4;
                        let locked_data = unsafe {
                            slice::from_raw_parts_mut(lock.as_mut_ptr() as *mut u8, total_length)
                        };
                        locked_data.copy_from_slice(data);
                    }
                    Ok(PackedImageTransfer::HardwareBuffer(unsafe {
                        owned.to_java_hardware_buffer(env)
                    }))
                }
            } else {
                Ok(PackedImageTransfer::Image(PackedImageBuffer {
                    data: data.to_vec(),
                    width,
                    height,
                    format: WeaverPreferredColorConfig::Rgba8888,
                }))
            }
        }
    }
}

fn make_10_12bit_transfer(
    env: &mut Env,
    data: &[u16],
    width: usize,
    height: usize,
    bit_depth: BitDepth,
    preferred_color: WeaverPreferredColorConfig,
) -> Result<PackedImageTransfer, anyhow::Error> {
    let format_8_bit = || {
        let diff = bit_depth.bits().saturating_sub(8) as u32;
        Ok(PackedImageTransfer::Image(PackedImageBuffer {
            data: data
                .iter()
                .map(|x| (x >> diff).min(255) as u8)
                .collect::<Vec<u8>>(),
            width,
            height,
            format: WeaverPreferredColorConfig::Rgba8888,
        }))
    };
    match preferred_color {
        WeaverPreferredColorConfig::Default | WeaverPreferredColorConfig::Rgba8888 => {
            format_8_bit()
        }
        WeaverPreferredColorConfig::RgbaF16 => {
            if android_os_version() >= MIN_OS_F16 {
                Ok(PackedImageTransfer::Image(PackedImageBuffer {
                    data: pack_10_or_12_to_f16(data, width, height, bit_depth.bits() as usize)?,
                    width,
                    height,
                    format: WeaverPreferredColorConfig::RgbaF16,
                }))
            } else {
                format_8_bit()
            }
        }
        WeaverPreferredColorConfig::Rgb565 => Ok(PackedImageTransfer::Image(PackedImageBuffer {
            data: pack_10_or_12_to_565(data, width, height, bit_depth.bits() as usize)?,
            width,
            height,
            format: WeaverPreferredColorConfig::Rgb565,
        })),
        WeaverPreferredColorConfig::Rgba1010102 => {
            if android_os_version() >= MIN_OS_AR30 {
                Ok(PackedImageTransfer::Image(PackedImageBuffer {
                    data: pack_10_or_12_to_ar30(data, width, height, bit_depth.bits() as usize)?,
                    width,
                    height,
                    format: WeaverPreferredColorConfig::Rgba1010102,
                }))
            } else {
                format_8_bit()
            }
        }
        WeaverPreferredColorConfig::Hardware => {
            if android_os_version() >= MIN_HARDWARE_BUFFER_OS
                && let Some(hw_apis) = load_hardware_buffer_api()
            {
                let usage = AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN.0
                    | AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN.0
                    | AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE.0
                    | AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT.0;
                let descriptor = AHardwareBuffer_Desc {
                    width: width as u32,
                    height: height as u32,
                    layers: 1,
                    format: AHardwareBuffer_Format::AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM.0,
                    usage,
                    stride: 0,
                    rfu0: 0,
                    rfu1: 0,
                };
                if !hw_apis.is_supported(&descriptor) {
                    format_8_bit()
                } else {
                    let mut owned = hw_apis.allocate_owned(&descriptor).map_err(|x| {
                        anyhow::anyhow!("Allocation hardware buffer failed with an error {x}")
                    })?;
                    let usage_flags =
                        AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN.0
                            | AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN.0;
                    let a_rect = ARect {
                        left: 0,
                        top: 0,
                        right: width as i32,
                        bottom: height as i32,
                    };
                    {
                        let mut lock = owned.lock(usage_flags, -1, Some(&a_rect)).map_err(|x| {
                            anyhow::anyhow!("Locking hardware buffer failed with an error {x}")
                        })?;
                        let total_length = width * height * 4;
                        let locked_data = unsafe {
                            slice::from_raw_parts_mut(lock.as_mut_ptr() as *mut u8, total_length)
                        };
                        locked_data.copy_from_slice(bytemuck::cast_slice(data));
                    }
                    Ok(PackedImageTransfer::HardwareBuffer(unsafe {
                        owned.to_java_hardware_buffer(env)
                    }))
                }
            } else {
                format_8_bit()
            }
        }
    }
}

fn decode_pipeline<'local>(
    env: &mut Env<'local>,
    data: &[u8],
    scaled_width: i32,
    scaled_height: i32,
    scale_mode: WeaveScaleMode,
    preferred_color: WeaverPreferredColorConfig,
) -> Result<JObject<'local>, anyhow::Error> {
    let decode = decode_packed_av2(data).map_err(|x| anyhow::anyhow!(x))?;
    let mut color_transform_done = false;
    match decode {
        PackedAv2::Regular(regular_image) => {
            dbg_log!(
                debug,
                "Regular 8-bit image: {}x{}, has_alpha={}, colors={:?}",
                regular_image.width,
                regular_image.height,
                regular_image.has_real_alpha,
                regular_image.colors
            );

            let mut scaled_data = internal_scale_u8(
                std::borrow::Cow::Borrowed(&regular_image.data),
                regular_image.width as u32 * 4,
                regular_image.width as u32,
                regular_image.height as u32,
                scaled_width,
                scaled_height,
                regular_image.has_real_alpha,
                scale_mode,
            )?;

            dbg_log!(
                debug,
                "scaled to {}x{}",
                scaled_data.width,
                scaled_data.height
            );

            if let Some(icc) = regular_image.icc.as_ref()
                && let Ok(transform) = ColorProfile::new_from_slice(icc).and_then(|x| {
                    x.create_transform_8bit(
                        Layout::Rgba,
                        &ColorProfile::new_srgb(),
                        Layout::Rgba,
                        TransformOptions::default(),
                    )
                })
            {
                dbg_log!(debug, "8-bit: applying ICC profile transform");
                let mut target_data = try_vec![0u8; scaled_data.buffer.borrow().len()];
                transform
                    .transform(scaled_data.buffer.borrow(), &mut target_data)
                    .map_err(|x| {
                        dbg_log!(error, "ICC transform failed: {x}");
                        anyhow::anyhow!(x)
                    })?;
                color_transform_done = true;
                scaled_data.buffer = pic_scale::BufferStore::Owned(target_data);
            } else if android_os_version() < 34
                && let Some(profile) = resolve_cicp_profile(regular_image.colors)
                && let Ok(transform) = profile.create_transform_8bit(
                    Layout::Rgba,
                    &ColorProfile::new_srgb(),
                    Layout::Rgba,
                    TransformOptions::default(),
                )
            {
                dbg_log!(debug, "8-bit: applying CICP profile transform (OS < 34)");
                let mut target_data = try_vec![0u8; scaled_data.buffer.borrow().len()];
                transform
                    .transform(scaled_data.buffer.borrow(), &mut target_data)
                    .map_err(|x| {
                        dbg_log!(error, "CICP transform failed: {x}");
                        anyhow::anyhow!(x)
                    })?;
                scaled_data.buffer = pic_scale::BufferStore::Owned(target_data);
                color_transform_done = true;
            } else {
                dbg_log!(
                    debug,
                    "8-bit: no color transform applied \
                    (color_transform_done={color_transform_done}, os={}, cicp={:?})",
                    android_os_version(),
                    regular_image.colors
                );
            }

            let new_width = scaled_data.width;
            let new_height = scaled_data.height;

            let transfer = make_8bit_transfer(
                env,
                scaled_data.buffer.borrow(),
                new_width,
                new_height,
                preferred_color,
            )?;

            let color_space = if !color_transform_done
                && android_os_version() >= MIN_OS_BITMAP_COLOR_SPACE
                && let Some(space) = resolve_native_profile(regular_image.colors)
            {
                dbg_log!(debug, "8-bit: attaching native color space {:?}", space);
                Some(space)
            } else {
                dbg_log!(
                    debug,
                    "8-bit: no native color space attached \
                    (transform_done={color_transform_done}, os={})",
                    android_os_version()
                );
                None
            };

            let result = Ok(match transfer {
                PackedImageTransfer::Image(image) => {
                    dbg_log!(
                        debug,
                        "8-bit: creating software bitmap {}x{} fmt={:?}",
                        image.width,
                        image.height,
                        image.format
                    );
                    software_bitmap(env, &image, color_space).map_err(|x| {
                        dbg_log!(error, "software_bitmap failed: {x}");
                        anyhow::anyhow!(x)
                    })?
                }
                PackedImageTransfer::HardwareBuffer(hb) => {
                    dbg_log!(debug, "8-bit: wrapping hardware buffer");
                    wrap_hardware_buffer(env, hb, color_space).map_err(|x| {
                        dbg_log!(error, "wrap_hardware_buffer failed: {x}");
                        anyhow::anyhow!(x)
                    })?
                }
            });

            dbg_log!(debug, "8-bit: decode_pipeline complete");
            result
        }
        PackedAv2::HighBitDepth(hd_image) => {
            let mut scaled_data = internal_scale_u16(
                std::borrow::Cow::Borrowed(&hd_image.data),
                hd_image.width * 4,
                hd_image.width as u32,
                hd_image.height as u32,
                scaled_width,
                scaled_height,
                hd_image.bit_depth.bits() as usize,
                hd_image.has_real_alpha,
                scale_mode,
            )?;
            match hd_image.bit_depth {
                BitDepth::Eight => unreachable!("Handled somewhere in different place"),
                BitDepth::Ten => {
                    if let Some(icc) = hd_image.icc.as_ref()
                        && let Ok(transform) = ColorProfile::new_from_slice(icc).and_then(|x| {
                            x.create_transform_10bit(
                                Layout::Rgba,
                                &ColorProfile::new_srgb(),
                                Layout::Rgba,
                                TransformOptions::default(),
                            )
                        })
                    {
                        let mut target_data = try_vec![0u16; scaled_data.buffer.borrow().len()];
                        transform
                            .transform(scaled_data.buffer.borrow(), &mut target_data)
                            .map_err(|x| anyhow::anyhow!(x))?;
                        scaled_data.buffer = pic_scale::BufferStore::Owned(target_data);
                        color_transform_done = true;
                    } else if android_os_version() < 34
                        && let Some(profile) = resolve_cicp_profile(hd_image.colors)
                        && let Ok(transform) = profile.create_transform_10bit(
                            Layout::Rgba,
                            &ColorProfile::new_srgb(),
                            Layout::Rgba,
                            TransformOptions::default(),
                        )
                    {
                        let mut target_data = try_vec![0u16; scaled_data.buffer.borrow().len()];
                        transform
                            .transform(scaled_data.buffer.borrow(), &mut target_data)
                            .map_err(|x| anyhow::anyhow!(x))?;
                        scaled_data.buffer = pic_scale::BufferStore::Owned(target_data);
                        color_transform_done = true;
                    }
                }
                BitDepth::Twelve => {
                    if let Some(icc) = hd_image.icc.as_ref()
                        && let Ok(transform) = ColorProfile::new_from_slice(icc).and_then(|x| {
                            x.create_transform_12bit(
                                Layout::Rgba,
                                &ColorProfile::new_srgb(),
                                Layout::Rgba,
                                TransformOptions::default(),
                            )
                        })
                    {
                        let mut target_data = try_vec![0u16; scaled_data.buffer.borrow().len()];
                        transform
                            .transform(scaled_data.buffer.borrow(), &mut target_data)
                            .map_err(|x| anyhow::anyhow!(x))?;
                        scaled_data.buffer = pic_scale::BufferStore::Owned(target_data);
                        color_transform_done = true;
                    } else if android_os_version() < 34
                        && let Some(profile) = resolve_cicp_profile(hd_image.colors)
                        && let Ok(transform) = profile.create_transform_12bit(
                            Layout::Rgba,
                            &ColorProfile::new_srgb(),
                            Layout::Rgba,
                            TransformOptions::default(),
                        )
                    {
                        let mut target_data = try_vec![0u16; scaled_data.buffer.borrow().len()];
                        transform
                            .transform(scaled_data.buffer.borrow(), &mut target_data)
                            .map_err(|x| anyhow::anyhow!(x))?;
                        scaled_data.buffer = pic_scale::BufferStore::Owned(target_data);
                        color_transform_done = true;
                    }
                }
            }

            let new_width = scaled_data.width;
            let new_height = scaled_data.height;
            let transfer = make_10_12bit_transfer(
                env,
                scaled_data.buffer.borrow(),
                new_width,
                new_height,
                hd_image.bit_depth,
                preferred_color,
            )?;
            let color_space = if !color_transform_done
                && android_os_version() >= MIN_OS_BITMAP_COLOR_SPACE
                && let Some(space) = resolve_native_profile(hd_image.colors)
            {
                Some(space)
            } else {
                None
            };

            Ok(match transfer {
                PackedImageTransfer::Image(image) => {
                    software_bitmap(env, &image, color_space).map_err(|x| anyhow::anyhow!(x))?
                }
                PackedImageTransfer::HardwareBuffer(hb) => {
                    wrap_hardware_buffer(env, hb, color_space).map_err(|x| anyhow::anyhow!(x))?
                }
            })
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn decode_av2_file(
    env: *mut jni::sys::JNIEnv,
    data: *const u8,
    length: usize,
    scaled_width: i32,
    scaled_height: i32,
    scale_mode: WeaveScaleMode,
    preferred_color_config: WeaverPreferredColorConfig,
) -> jobject {
    init_logging();

    dbg_log!(
        debug,
        "decode_heic_file called: length={length}, \
        target={}x{}, scale={:?}, color={:?}",
        scaled_width,
        scaled_height,
        scale_mode,
        preferred_color_config
    );

    let mut unowned = unsafe { EnvUnowned::from_raw(env) };

    let outcome = unowned.with_env(|env| -> Result<jobject, jni::errors::Error> {
        let bytes = unsafe { slice::from_raw_parts(data, length) };

        let mut decoding_result = || -> Result<jobject, jni::errors::Error> {
            match decode_pipeline(
                env,
                bytes,
                scaled_width,
                scaled_height,
                scale_mode,
                preferred_color_config,
            ) {
                Ok(obj) => {
                    dbg_log!(debug, "decode_heic_file succeeded");
                    Ok(obj.as_raw())
                }
                Err(e) => {
                    dbg_log!(error, "decode_heic_file failed: {e:#}");
                    let _ = env.throw_new(
                        jni_str!("java/lang/RuntimeException"),
                        JNIString::from(e.to_string()),
                    );
                    Ok(JObject::null().as_raw())
                }
            }
        };
        let result = decoding_result();
        match result {
            Ok(obj) => {
                dbg_log!(debug, "decode_heic_file: success");
                Ok(obj)
            }
            Err(e) => {
                dbg_log!(error, "decode_heic_file failed: {e:#}");
                let _ = env.throw_new(
                    jni_str!("java/lang/RuntimeException"),
                    JNIString::from(e.to_string()),
                );
                Ok(JObject::null().into_raw())
            }
        }
    });

    let o = outcome.into_outcome();
    match o {
        Outcome::Ok(v) => v,
        Outcome::Err(_e) => {
            dbg_log!(error, "JNI error in with_env: {_e:?}");
            null_mut()
        }
        Outcome::Panic(_p) => {
            dbg_log!(error, "panic in with_env: {_p:?}");
            null_mut()
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn read_av2_file_info(data: *const u8, length: usize) -> HeicInfo {
    if unsafe { !is_av2_image(data, length) } {
        return HeicInfo::not_a_heic();
    }
    let bytes = unsafe { slice::from_raw_parts(data, length) };
    let image_info = match AvifDecoder::new(bytes).and_then(|x| x.image_info()) {
        Ok(v) => v,
        Err(_v) => {
            dbg_log!(error, "Failed to read AV2 info: {_v:?}");
            return HeicInfo::not_a_heic();
        }
    };

    let (width, height) = match image_info
        .orientation
        .unwrap_or(tealdust::Orientation::Normal)
    {
        tealdust::Orientation::Rotate90
        | tealdust::Orientation::Rotate270
        | tealdust::Orientation::Transpose
        | tealdust::Orientation::Transverse => (image_info.height, image_info.width),
        _ => (image_info.width, image_info.height),
    };
    HeicInfo {
        width,
        height,
        supported_image: true,
        bit_depth: image_info.bits_per_component as u32,
    }
}
