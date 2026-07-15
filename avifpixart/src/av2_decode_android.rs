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
use crate::av2_decode::{DecodedAvifPacket, PackedAv2, decode_packed_av2};
use crate::cvt::{
    pack_8_to_565, pack_8_to_ar30, pack_8_to_f16, pack_10_or_12_to_565, pack_10_or_12_to_ar30,
    pack_10_or_12_to_f16,
};
use crate::ffi::{
    MIN_OS_BITMAP_COLOR_SPACE, create_rgba8888_hardware_buffer,
    create_rgba8888_hardware_buffer_from_u16, software_bitmap, wrap_hardware_buffer,
};
use crate::heic_decode::WeaverError;
use crate::native_color_space::NativeColorSpace;
use crate::scaling::{internal_scale_u8, internal_scale_u16};
use crate::support::{
    MIN_OS_AR30, MIN_OS_F16, PackedImageBuffer, PackedImageTransfer, android_os_version, dbg_log,
    init_logging, try_vec,
};
use crate::{HeicInfo, WeaveScaleMode, WeaverPreferredColorConfig, is_av2_image};
use gainforge::{
    GainHdrMetadata, GamutClipping, MappingColorSpace, RgbToneMapperParameters, ToneMappingMethod,
    create_tone_mapper_rgba, create_tone_mapper_rgba10, create_tone_mapper_rgba12,
};
use hpvcd::BitDepth;
use jni::objects::JObject;
use jni::strings::JNIString;
use jni::sys::jobject;
use jni::{Env, EnvUnowned, Outcome, jni_str};
use moxcms::{ColorProfile, Layout, TransformOptions};
use std::ptr::null_mut;
use std::slice;
use tealdust::{AvifDecoder, AvifSettings, ColorInfo};

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
            match create_rgba8888_hardware_buffer(env, data, width, height) {
                Ok(Some(hardware_buffer)) => {
                    Ok(PackedImageTransfer::HardwareBuffer(hardware_buffer))
                }
                Ok(None) => Ok(PackedImageTransfer::Image(PackedImageBuffer {
                    data: data.to_vec(),
                    width,
                    height,
                    format: WeaverPreferredColorConfig::Rgba8888,
                })),
                Err(_error) => {
                    dbg_log!(
                        warn,
                        "Hardware RGBA8888 transfer failed for {}x{}; falling back to software bitmap: {_error:#}",
                        width,
                        height
                    );
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
            match create_rgba8888_hardware_buffer_from_u16(
                env,
                data,
                width,
                height,
                bit_depth.bits() as usize,
            ) {
                Ok(Some(hardware_buffer)) => {
                    Ok(PackedImageTransfer::HardwareBuffer(hardware_buffer))
                }
                Ok(None) => format_8_bit(),
                Err(_error) => {
                    dbg_log!(
                        warn,
                        "Hardware high-bit-depth transfer failed for {}x{} at {} bits; falling back to software bitmap: {_error:#}",
                        width,
                        height,
                        bit_depth.bits()
                    );
                    format_8_bit()
                }
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
            let DecodedAvifPacket {
                data: source_data,
                width,
                height,
                colors,
                icc,
                bit_depth: _,
                has_real_alpha,
                clli,
            } = regular_image;
            let _source_len = source_data.len();
            dbg_log!(
                debug,
                "Regular 8-bit image: {}x{}, has_alpha={}, colors={:?}, source_bytes={}, requested={}x{}, mode={:?}",
                width,
                height,
                has_real_alpha,
                colors,
                _source_len,
                scaled_width,
                scaled_height,
                scale_mode
            );

            let mut scaled_data = internal_scale_u8(
                std::borrow::Cow::Owned(source_data),
                width as u32 * 4,
                width as u32,
                height as u32,
                scaled_width,
                scaled_height,
                has_real_alpha,
                scale_mode,
            )?;

            dbg_log!(
                debug,
                "8-bit scaling complete: source_bytes_released={}, output={}x{}, output_bytes={}",
                _source_len,
                scaled_data.width,
                scaled_data.height,
                scaled_data.buffer.borrow().len()
            );

            if let Some(icc) = icc.as_ref()
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
            } else if android_os_version() < 34 {
                let tc = colors.transfer_characteristics;
                if (tc != tealdust::TransferCharacteristics::Smpte2084
                    && tc != tealdust::TransferCharacteristics::Hlg)
                    && let Some(profile) = resolve_cicp_profile(colors)
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
                } else if tc == tealdust::TransferCharacteristics::Smpte2084
                    || tc == tealdust::TransferCharacteristics::Hlg
                {
                    let mut new_profile = ColorProfile::default();
                    let colorimetry = match colors.color_primaries {
                        tealdust::ColorPrimaries::Bt709 => moxcms::ColorPrimaries::BT_709,
                        tealdust::ColorPrimaries::Bt2020 => moxcms::ColorPrimaries::BT_2020,
                        tealdust::ColorPrimaries::Smpte431 => moxcms::ColorPrimaries::DCI_P3,
                        tealdust::ColorPrimaries::Smpte432 => moxcms::ColorPrimaries::DISPLAY_P3,
                        _ => moxcms::ColorPrimaries::BT_601,
                    };
                    new_profile.update_rgb_colorimetry_triplet(
                        if colors.color_primaries == tealdust::ColorPrimaries::Smpte431 {
                            moxcms::WHITE_POINT_D60
                        } else {
                            moxcms::WHITE_POINT_D65
                        },
                        colorimetry.red.to_xyzd(),
                        colorimetry.green.to_xyzd(),
                        colorimetry.blue.to_xyzd(),
                    );
                    new_profile.cicp = None;
                    if let Ok(tone_mapper) = create_tone_mapper_rgba(
                        &new_profile,
                        &ColorProfile::new_srgb(),
                        ToneMappingMethod::TunedReinhard(GainHdrMetadata {
                            display_max_brightness: 203f32,
                            content_max_brightness: clli.max_content_light_level as f32,
                        }),
                        MappingColorSpace::Rgb(RgbToneMapperParameters {
                            gamut_clipping: GamutClipping::Clip,
                            exposure: 1.0,
                        }),
                    ) {
                        let mut target_data = try_vec![0u8; scaled_data.buffer.borrow().len()];
                        tone_mapper
                            .tonemap_lane(scaled_data.buffer.borrow(), &mut target_data)
                            .map_err(|x| {
                                dbg_log!(error, "CICP transform failed: {x}");
                                anyhow::anyhow!(x)
                            })?;
                        scaled_data.buffer = pic_scale::BufferStore::Owned(target_data);
                        color_transform_done = true;
                    }
                }
            } else {
                dbg_log!(
                    debug,
                    "8-bit: no color transform applied \
                    (color_transform_done={color_transform_done}, os={}, cicp={:?})",
                    android_os_version(),
                    colors
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
                && let Some(space) = resolve_native_profile(colors)
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
            let DecodedAvifPacket {
                data: source_data,
                width,
                height,
                colors,
                icc,
                bit_depth,
                has_real_alpha,
                clli,
            } = hd_image;
            let _source_samples = source_data.len();
            dbg_log!(
                debug,
                "High-bit-depth image: {}x{}, depth={}, has_alpha={}, colors={:?}, source_samples={}, requested={}x{}, mode={:?}",
                width,
                height,
                bit_depth.bits(),
                has_real_alpha,
                colors,
                _source_samples,
                scaled_width,
                scaled_height,
                scale_mode
            );

            let mut scaled_data = internal_scale_u16(
                std::borrow::Cow::Owned(source_data),
                width * 4,
                width as u32,
                height as u32,
                scaled_width,
                scaled_height,
                bit_depth.bits() as usize,
                has_real_alpha,
                scale_mode,
            )?;
            dbg_log!(
                debug,
                "High-bit-depth scaling complete: source_samples_released={}, output={}x{}, output_samples={}",
                _source_samples,
                scaled_data.width,
                scaled_data.height,
                scaled_data.buffer.borrow().len()
            );
            match bit_depth {
                BitDepth::Eight => unreachable!("Handled somewhere in different place"),
                BitDepth::Ten => {
                    if let Some(icc) = icc.as_ref()
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
                    } else if android_os_version() < 34 {
                        let tc = colors.transfer_characteristics;
                        if (tc != tealdust::TransferCharacteristics::Smpte2084
                            && tc != tealdust::TransferCharacteristics::Hlg)
                            && let Some(profile) = resolve_cicp_profile(colors)
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
                        } else if tc == tealdust::TransferCharacteristics::Smpte2084
                            || tc == tealdust::TransferCharacteristics::Hlg
                        {
                            let mut new_profile = ColorProfile::default();
                            let colorimetry = match colors.color_primaries {
                                tealdust::ColorPrimaries::Bt709 => moxcms::ColorPrimaries::BT_709,
                                tealdust::ColorPrimaries::Bt2020 => moxcms::ColorPrimaries::BT_2020,
                                tealdust::ColorPrimaries::Smpte431 => {
                                    moxcms::ColorPrimaries::DCI_P3
                                }
                                tealdust::ColorPrimaries::Smpte432 => {
                                    moxcms::ColorPrimaries::DISPLAY_P3
                                }
                                _ => moxcms::ColorPrimaries::BT_601,
                            };
                            new_profile.update_rgb_colorimetry_triplet(
                                if colors.color_primaries == tealdust::ColorPrimaries::Smpte431 {
                                    moxcms::WHITE_POINT_D60
                                } else {
                                    moxcms::WHITE_POINT_D65
                                },
                                colorimetry.red.to_xyzd(),
                                colorimetry.green.to_xyzd(),
                                colorimetry.blue.to_xyzd(),
                            );
                            new_profile.cicp = None;
                            if let Ok(tone_mapper) = create_tone_mapper_rgba10(
                                &new_profile,
                                &ColorProfile::new_srgb(),
                                ToneMappingMethod::TunedReinhard(GainHdrMetadata {
                                    display_max_brightness: 203f32,
                                    content_max_brightness: clli.max_content_light_level as f32,
                                }),
                                MappingColorSpace::Rgb(RgbToneMapperParameters {
                                    gamut_clipping: GamutClipping::Clip,
                                    exposure: 1.0,
                                }),
                            ) {
                                let mut target_data =
                                    try_vec![0u16; scaled_data.buffer.borrow().len()];
                                tone_mapper
                                    .tonemap_lane(scaled_data.buffer.borrow(), &mut target_data)
                                    .map_err(|x| {
                                        dbg_log!(error, "CICP transform failed: {x}");
                                        anyhow::anyhow!(x)
                                    })?;
                                scaled_data.buffer = pic_scale::BufferStore::Owned(target_data);
                                color_transform_done = true;
                            }
                        }
                    }
                }
                BitDepth::Twelve => {
                    if let Some(icc) = icc.as_ref()
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
                    } else if android_os_version() < 34 {
                        let tc = colors.transfer_characteristics;
                        if (tc != tealdust::TransferCharacteristics::Smpte2084
                            && tc != tealdust::TransferCharacteristics::Hlg)
                            && let Some(profile) = resolve_cicp_profile(colors)
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
                        } else if tc == tealdust::TransferCharacteristics::Smpte2084
                            || tc == tealdust::TransferCharacteristics::Hlg
                        {
                            let mut new_profile = ColorProfile::default();
                            let colorimetry = match colors.color_primaries {
                                tealdust::ColorPrimaries::Bt709 => moxcms::ColorPrimaries::BT_709,
                                tealdust::ColorPrimaries::Bt2020 => moxcms::ColorPrimaries::BT_2020,
                                tealdust::ColorPrimaries::Smpte431 => {
                                    moxcms::ColorPrimaries::DCI_P3
                                }
                                tealdust::ColorPrimaries::Smpte432 => {
                                    moxcms::ColorPrimaries::DISPLAY_P3
                                }
                                _ => moxcms::ColorPrimaries::BT_601,
                            };
                            new_profile.update_rgb_colorimetry_triplet(
                                if colors.color_primaries == tealdust::ColorPrimaries::Smpte431 {
                                    moxcms::WHITE_POINT_D60
                                } else {
                                    moxcms::WHITE_POINT_D65
                                },
                                colorimetry.red.to_xyzd(),
                                colorimetry.green.to_xyzd(),
                                colorimetry.blue.to_xyzd(),
                            );
                            new_profile.cicp = None;
                            if let Ok(tone_mapper) = create_tone_mapper_rgba12(
                                &new_profile,
                                &ColorProfile::new_srgb(),
                                ToneMappingMethod::TunedReinhard(GainHdrMetadata {
                                    display_max_brightness: 203f32,
                                    content_max_brightness: clli.max_content_light_level as f32,
                                }),
                                MappingColorSpace::Rgb(RgbToneMapperParameters {
                                    gamut_clipping: GamutClipping::Clip,
                                    exposure: 1.0,
                                }),
                            ) {
                                let mut target_data =
                                    try_vec![0u16; scaled_data.buffer.borrow().len()];
                                tone_mapper
                                    .tonemap_lane(scaled_data.buffer.borrow(), &mut target_data)
                                    .map_err(|x| {
                                        dbg_log!(error, "CICP transform failed: {x}");
                                        anyhow::anyhow!(x)
                                    })?;
                                scaled_data.buffer = pic_scale::BufferStore::Owned(target_data);
                                color_transform_done = true;
                            }
                        }
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
                bit_depth,
                preferred_color,
            )?;
            let color_space = if !color_transform_done
                && android_os_version() >= MIN_OS_BITMAP_COLOR_SPACE
                && let Some(space) = resolve_native_profile(colors)
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
            let _msg = _p
                .downcast_ref::<&str>()
                .map(|s| s.to_string())
                .or_else(|| _p.downcast_ref::<String>().cloned())
                .unwrap_or_else(|| "unknown panic".to_string());
            dbg_log!(error, "panic in with_env: {_msg}");
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
    let mut settings = AvifSettings::default();
    settings.decoder_settings.frame_size_limit = 16536 * 16536;
    let image_info = match AvifDecoder::with_settings(bytes, settings).and_then(|x| x.image_info())
    {
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
