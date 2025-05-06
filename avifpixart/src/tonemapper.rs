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
use crate::cvt::work_on_transmuted_ptr_u16;
use crate::icc::{apply_icc_rgba16_impl, apply_icc_rgba8_impl};
use gainforge::{
    create_tone_mapper_rgba, create_tone_mapper_rgba10, create_tone_mapper_rgba12,
    create_tone_mapper_rgba16, CommonToneMapperParameters, GainHdrMetadata, GamutClipping,
    MappingColorSpace, RgbToneMapperParameters, ToneMappingMethod,
};
use moxcms::{
    Chromaticity, CicpColorPrimaries, CicpProfile, ColorProfile, MatrixCoefficients,
    TransferCharacteristics, XyY,
};

#[repr(C)]
pub enum FfiTrc {
    /// For future use by ITU-T | ISO/IEC
    Reserved,
    /// Rec. ITU-R BT.709-6<br />
    /// Rec. ITU-R BT.1361-0 conventional colour gamut system (historical)<br />
    /// (functionally the same as the values 6, 14 and 15)    <br />
    Bt709 = 1,
    /// Image characteristics are unknown or are determined by the application.<br />
    Unspecified = 2,
    /// Rec. ITU-R BT.470-6 System M (historical)<br />
    /// United States National Television System Committee 1953 Recommendation for transmission standards for color television<br />
    /// United States Federal Communications Commission (2003) Title 47 Code of Federal Regulations 73.682 (a) (20)<br />
    /// Rec. ITU-R BT.1700-0 625 PAL and 625 SECAM<br />
    Bt470M = 4,
    /// Rec. ITU-R BT.470-6 System B, G (historical)<br />
    Bt470Bg = 5,
    /// Rec. ITU-R BT.601-7 525 or 625<br />
    /// Rec. ITU-R BT.1358-1 525 or 625 (historical)<br />
    /// Rec. ITU-R BT.1700-0 NTSC SMPTE 170M (2004)<br />
    /// (functionally the same as the values 1, 14 and 15)<br />
    Bt601 = 6,
    /// SMPTE 240M (1999) (historical)<br />
    Smpte240 = 7,
    /// Linear transfer characteristics<br />
    Linear = 8,
    /// Logarithmic transfer characteristic (100:1 range)<br />
    Log100 = 9,
    /// Logarithmic transfer characteristic (100 * Sqrt( 10 ) : 1 range)<br />
    Log100sqrt10 = 10,
    /// IEC 61966-2-4<br />
    Iec61966 = 11,
    /// Rec. ITU-R BT.1361-0 extended colour gamut system (historical)<br />
    Bt1361 = 12,
    /// IEC 61966-2-1 sRGB or sYCC<br />
    Srgb = 13,
    /// Rec. ITU-R BT.2020-2 (10-bit system)<br />
    /// (functionally the same as the values 1, 6 and 15)<br />
    Bt202010bit = 14,
    /// Rec. ITU-R BT.2020-2 (12-bit system)<br />
    /// (functionally the same as the values 1, 6 and 14)<br />
    Bt202012bit = 15,
    /// SMPTE ST 2084 for 10-, 12-, 14- and 16-bitsystems<br />
    /// Rec. ITU-R BT.2100-0 perceptual quantization (PQ) system<br />
    Smpte2084 = 16,
    /// SMPTE ST 428-1<br />
    Smpte428 = 17,
    /// ARIB STD-B67<br />
    /// Rec. ITU-R BT.2100-0 hybrid log- gamma (HLG) system<br />
    Hlg = 18,
}

impl FfiTrc {
    pub fn to_characteristics(&self) -> TransferCharacteristics {
        match self {
            FfiTrc::Reserved => TransferCharacteristics::Reserved,
            FfiTrc::Bt709 => TransferCharacteristics::Bt709,
            FfiTrc::Unspecified => TransferCharacteristics::Unspecified,
            FfiTrc::Bt470M => TransferCharacteristics::Bt470M,
            FfiTrc::Bt470Bg => TransferCharacteristics::Bt470Bg,
            FfiTrc::Bt601 => TransferCharacteristics::Bt601,
            FfiTrc::Smpte240 => TransferCharacteristics::Smpte240,
            FfiTrc::Linear => TransferCharacteristics::Linear,
            FfiTrc::Log100 => TransferCharacteristics::Log100,
            FfiTrc::Log100sqrt10 => TransferCharacteristics::Log100sqrt10,
            FfiTrc::Iec61966 => TransferCharacteristics::Iec61966,
            FfiTrc::Bt1361 => TransferCharacteristics::Bt1361,
            FfiTrc::Srgb => TransferCharacteristics::Srgb,
            FfiTrc::Bt202010bit => TransferCharacteristics::Bt202010bit,
            FfiTrc::Bt202012bit => TransferCharacteristics::Bt202012bit,
            FfiTrc::Smpte2084 => TransferCharacteristics::Smpte2084,
            FfiTrc::Smpte428 => TransferCharacteristics::Smpte428,
            FfiTrc::Hlg => TransferCharacteristics::Hlg,
        }
    }
}

#[repr(C)]
pub enum ToneMapping {
    Skip,
    Rec2408,
}

#[no_mangle]
pub unsafe extern "C" fn apply_tone_mapping_rgba8(
    image: *mut u8,
    stride: u32,
    width: u32,
    height: u32,
    primaries: *const f32,
    white_point: *const f32,
    trc: FfiTrc,
    mapping: ToneMapping,
    brightness: f32,
) {
    unsafe {
        let red_chromaticity = Chromaticity::new(
            primaries.read_unaligned(),
            primaries.add(1).read_unaligned(),
        );
        let green_chromaticity = Chromaticity::new(
            primaries.add(2).read_unaligned(),
            primaries.add(3).read_unaligned(),
        );
        let blue_chromaticity = Chromaticity::new(
            primaries.add(4).read_unaligned(),
            primaries.add(5).read_unaligned(),
        );
        let white_point = XyY {
            x: white_point.read_unaligned() as f64,
            y: white_point.add(1).read_unaligned() as f64,
            yb: 1.0,
        };

        let trc = trc.to_characteristics();

        let mut new_profile = ColorProfile::default();
        new_profile.update_rgb_colorimetry_triplet(
            white_point,
            red_chromaticity.to_xyzd(),
            blue_chromaticity.to_xyzd(),
            green_chromaticity.to_xyzd(),
        );
        new_profile.cicp = Some(CicpProfile {
            full_range: true,
            color_primaries: CicpColorPrimaries::Bt709,
            transfer_characteristics: trc,
            matrix_coefficients: MatrixCoefficients::Bt709,
        });

        let dst_image = std::slice::from_raw_parts_mut(image, stride as usize * height as usize);
        let src_image = dst_image.to_vec();

        match mapping {
            ToneMapping::Skip => {
                apply_icc_rgba8_impl(&src_image, stride, dst_image, stride, width, new_profile);
            }
            ToneMapping::Rec2408 => {
                match create_tone_mapper_rgba(
                    &new_profile,
                    &ColorProfile::new_srgb(),
                    ToneMappingMethod::Rec2408(GainHdrMetadata {
                        display_max_brightness: 203f32,
                        content_max_brightness: brightness,
                    }),
                    MappingColorSpace::Rgb(RgbToneMapperParameters {
                        gamut_clipping: GamutClipping::Clip,
                        exposure: 1.0,
                    }),
                ) {
                    Ok(tone_mapper) => {
                        for (src_row, dst_row) in src_image
                            .chunks_exact(stride as usize)
                            .zip(dst_image.chunks_exact_mut(stride as usize))
                        {
                            let src = &src_row[..width as usize * 4];
                            let dst = &mut dst_row[..width as usize * 4];
                            tone_mapper.tonemap_lane(src, dst).unwrap();
                        }
                    }
                    Err(_) => {}
                }
            }
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn apply_tone_mapping_rgba16(
    image: *mut u16,
    stride: u32,
    bit_depth: u32,
    width: u32,
    height: u32,
    primaries: *const f32,
    white_point: *const f32,
    trc: FfiTrc,
    mapping: ToneMapping,
    brightness: f32,
) {
    unsafe {
        let red_chromaticity = Chromaticity::new(
            primaries.read_unaligned(),
            primaries.add(1).read_unaligned(),
        );
        let green_chromaticity = Chromaticity::new(
            primaries.add(2).read_unaligned(),
            primaries.add(3).read_unaligned(),
        );
        let blue_chromaticity = Chromaticity::new(
            primaries.add(4).read_unaligned(),
            primaries.add(5).read_unaligned(),
        );
        let white_point = XyY {
            x: white_point.read_unaligned() as f64,
            y: white_point.add(1).read_unaligned() as f64,
            yb: 1.0,
        };

        let trc = trc.to_characteristics();

        let mut new_profile = ColorProfile::default();
        new_profile.update_rgb_colorimetry_triplet(
            white_point,
            red_chromaticity.to_xyzd(),
            green_chromaticity.to_xyzd(),
            blue_chromaticity.to_xyzd(),
        );
        new_profile.cicp = Some(CicpProfile {
            full_range: true,
            color_primaries: CicpColorPrimaries::Bt709,
            transfer_characteristics: trc,
            matrix_coefficients: MatrixCoefficients::Bt709,
        });

        work_on_transmuted_ptr_u16(
            image,
            stride,
            width as usize,
            height as usize,
            true,
            |dst: &mut [u16], d_dst_stride: usize| {
                let src_image = dst.to_vec();
                match mapping {
                    ToneMapping::Skip => {
                        apply_icc_rgba16_impl(
                            &src_image,
                            d_dst_stride as u32,
                            dst,
                            d_dst_stride as u32,
                            width,
                            bit_depth,
                            &new_profile,
                        );
                    }
                    ToneMapping::Rec2408 => {
                        let mapper = if bit_depth == 10 {
                            create_tone_mapper_rgba10(
                                &new_profile,
                                &ColorProfile::new_srgb(),
                                ToneMappingMethod::Rec2408(GainHdrMetadata {
                                    display_max_brightness: 203.,
                                    content_max_brightness: brightness,
                                }),
                                MappingColorSpace::Rgb(RgbToneMapperParameters {
                                    gamut_clipping: GamutClipping::NoClip,
                                    exposure: 1.0,
                                }),
                            )
                        } else if bit_depth == 12 {
                            create_tone_mapper_rgba12(
                                &new_profile,
                                &ColorProfile::new_srgb(),
                                ToneMappingMethod::Rec2408(GainHdrMetadata {
                                    display_max_brightness: 203.,
                                    content_max_brightness: brightness,
                                }),
                                MappingColorSpace::Rgb(RgbToneMapperParameters {
                                    gamut_clipping: GamutClipping::NoClip,
                                    exposure: 1.0,
                                }),
                            )
                        } else {
                            create_tone_mapper_rgba16(
                                &new_profile,
                                &ColorProfile::new_srgb(),
                                ToneMappingMethod::Rec2408(GainHdrMetadata {
                                    display_max_brightness: 203.,
                                    content_max_brightness: brightness,
                                }),
                                MappingColorSpace::YRgb(CommonToneMapperParameters {
                                    gamut_clipping: GamutClipping::NoClip,
                                    exposure: 1.0,
                                }),
                            )
                        };
                        match mapper {
                            Ok(tone_mapper) => {
                                for (src_row, dst_row) in src_image
                                    .chunks_exact(d_dst_stride)
                                    .zip(dst.chunks_exact_mut(d_dst_stride))
                                {
                                    let src = &src_row[..width as usize * 4];
                                    let dst = &mut dst_row[..width as usize * 4];
                                    tone_mapper.tonemap_lane(src, dst).unwrap();
                                }
                            }
                            Err(_) => {}
                        }
                    }
                }
            },
        );
    }
}
