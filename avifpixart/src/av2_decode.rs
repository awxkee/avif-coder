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
use crate::check_image_size_overflow;
use crate::heic_decode::WeaverError;
use crate::orientation::apply_orientation_av2;
use crate::support::try_vec;
use hpvcd::BitDepth;
use tealdust::{AvifImage, ColorInfo, ContentLightLevel};
use yuv::{
    YuvGrayAlphaImage, YuvGrayImage, YuvPlanarImage, YuvPlanarImageWithAlpha, YuvStandardMatrix,
    i010_alpha_to_rgba10, i010_to_rgba10, i012_alpha_to_rgba12, i012_to_rgba12,
    i210_alpha_to_rgba10, i210_to_rgba10, i212_alpha_to_rgba12, i212_to_rgba12,
    i410_alpha_to_rgba10, i410_to_rgba10, i412_alpha_to_rgba12, i412_to_rgba12,
    icgc010_alpha_to_rgba10, icgc010_to_rgba10, icgc012_alpha_to_rgba12, icgc012_to_rgba12,
    icgc210_alpha_to_rgba10, icgc210_to_rgba10, icgc212_alpha_to_rgba12, icgc212_to_rgba12,
    icgc412_alpha_to_rgba12, icgc412_to_rgba12, y010_alpha_to_rgba10, y010_to_rgba10,
    y012_alpha_to_rgba12, y012_to_rgba12, ycgco420_alpha_to_rgba, ycgco420_to_rgba,
    ycgco422_alpha_to_rgba, ycgco422_to_rgba, yuv400_alpha_to_rgba, yuv400_to_rgba,
    yuv420_alpha_to_rgba, yuv420_to_rgba, yuv422_alpha_to_rgba, yuv422_to_rgba,
    yuv444_alpha_to_rgba, yuv444_to_rgba,
};

pub(crate) struct DecodedAvifPacket<T> {
    pub(crate) data: Vec<T>,
    pub(crate) width: usize,
    pub(crate) height: usize,
    pub(crate) colors: ColorInfo,
    pub(crate) icc: Option<Vec<u8>>,
    pub(crate) bit_depth: BitDepth,
    pub(crate) has_real_alpha: bool,
    pub(crate) clli: ContentLightLevel,
}

fn finalize<T: Copy + Default>(
    mut data: Vec<T>,
    image: &AvifImage,
    image_info: &tealdust::AvifImageInfo,
    cicp: ColorInfo,
) -> Result<DecodedAvifPacket<T>, WeaverError> {
    const CH: usize = 4; // RGBA
    let mut width = image.width as usize;
    let mut height = image.height as usize;

    // clap is defined in coded space, so crop BEFORE orientation (clap → irot → imir).
    if let Some((left, top, cw, ch)) = image
        .clean_aperture
        .as_ref()
        .and_then(|x| x.to_crop_rect(image.width, image.height))
        && (cw as usize != width || ch as usize != height)
    {
        let (src_stride, dst_stride) = (width * CH, cw as usize * CH);
        let mut cropped = try_vec![T::default(); cw as usize * ch as usize * CH];
        for row in 0..ch as usize {
            let s = (top as usize + row) * src_stride + left as usize * CH;
            let d = row * dst_stride;
            cropped[d..d + dst_stride].copy_from_slice(&data[s..s + dst_stride]);
        }
        data = cropped;
        width = cw as usize;
        height = ch as usize;
    }

    if image.orientation != tealdust::Orientation::Normal {
        let (nd, nw, nh) = apply_orientation_av2(&data, width, height, image.orientation);
        data = nd;
        width = nw;
        height = nh;
    }

    Ok(DecodedAvifPacket {
        data,
        width,
        height,
        colors: cicp,
        icc: image_info.icc_profile.as_ref().map(|x| x.to_vec()),
        bit_depth: match image.bits_per_component {
            8 => BitDepth::Eight,
            10 => BitDepth::Ten,
            12 => BitDepth::Twelve,
            _ => unreachable!("It should be impossible! and handled somewhere in different place"),
        },
        has_real_alpha: image.alpha.is_some(),
        clli: image_info.content_light_level.unwrap_or(ContentLightLevel {
            max_content_light_level: 2000,
            max_frame_average_light_level: 2000,
        }),
    })
}

pub(crate) enum PackedAv2 {
    Regular(DecodedAvifPacket<u8>),
    HighBitDepth(DecodedAvifPacket<u16>),
}

struct AvifColors {
    cicp: ColorInfo,
    matrix: YuvStandardMatrix,
    range: yuv::YuvRange,
}

fn solve_av2_colors(image: &AvifImage) -> Result<AvifColors, WeaverError> {
    let cicp = image.color_info.unwrap_or(ColorInfo {
        color_primaries: tealdust::ColorPrimaries::Bt709,
        transfer_characteristics: tealdust::TransferCharacteristics::Srgb,
        matrix_coefficients: tealdust::MatrixCoefficients::Bt601,
        full_range: true,
    });
    let yuv_range = match cicp.full_range {
        true => yuv::YuvRange::Limited,
        false => yuv::YuvRange::Full,
    };

    let matrix: YuvStandardMatrix = match cicp.matrix_coefficients {
        tealdust::MatrixCoefficients::Bt709 => YuvStandardMatrix::Bt709,
        tealdust::MatrixCoefficients::Unknown => YuvStandardMatrix::Bt601,
        tealdust::MatrixCoefficients::Bt601 => YuvStandardMatrix::Bt601,
        tealdust::MatrixCoefficients::Bt2020Ncl => YuvStandardMatrix::Bt2020,
        tealdust::MatrixCoefficients::Fcc => YuvStandardMatrix::Fcc,
        tealdust::MatrixCoefficients::YCgCo => YuvStandardMatrix::Custom(0.25, 0.25),
        v => return Err(WeaverError::UnsupportedMatrixAv2(v)),
    };
    Ok(AvifColors {
        cicp,
        range: yuv_range,
        matrix,
    })
}

fn is_metadata_ycgco(image: &AvifImage) -> bool {
    image
        .color_info
        .unwrap_or(tealdust::ColorInfo {
            color_primaries: tealdust::ColorPrimaries::Bt709,
            transfer_characteristics: tealdust::TransferCharacteristics::Srgb,
            matrix_coefficients: tealdust::MatrixCoefficients::Bt601,
            full_range: true,
        })
        .matrix_coefficients
        == tealdust::MatrixCoefficients::YCgCo
}

fn decode_inner_low_bit_depth(
    image: &AvifImage,
    image_info: &tealdust::AvifImageInfo,
) -> Result<DecodedAvifPacket<u8>, WeaverError> {
    if check_image_size_overflow(
        image.width as u64,
        image.height as u64,
        4,
        size_of::<u8>() as isize,
    ) {
        return Err(WeaverError::FailedToAllocateMemory(isize::MAX as u64));
    }

    let mut target_data = try_vec![0u8; image.width as usize * image.height as usize * 4];

    let colors = solve_av2_colors(image)?;
    let is_ycgco = is_metadata_ycgco(image);

    if image.pixel_layout == tealdust::PixelLayout::I400 {
        if let Some(alpha_plane) = image.alpha.as_ref() {
            let gray_alpha_img = YuvGrayAlphaImage {
                y_plane: &image.planes[0],
                y_stride: image.strides[0] as u32,
                a_plane: &alpha_plane.data,
                a_stride: alpha_plane.stride as u32,
                width: image.width,
                height: image.height,
            };
            yuv400_alpha_to_rgba(
                &gray_alpha_img,
                &mut target_data,
                image.width * 4,
                colors.range,
                colors.matrix,
            )
            .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;

            return finalize(target_data, image, image_info, colors.cicp);
        }

        let gray_img = YuvGrayImage {
            y_plane: &image.planes[0],
            y_stride: image.strides[0] as u32,
            width: image.width,
            height: image.height,
        };
        yuv400_to_rgba(
            &gray_img,
            &mut target_data,
            image.width * 4,
            colors.range,
            colors.matrix,
        )
        .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;

        return finalize(target_data, image, image_info, colors.cicp);
    }

    macro_rules! decode_chroma_to_rgba {
        ($to_rgba:ident, $alpha_to_rgba:ident, $sub_w: expr) => {{
            if let Some(alpha_plane) = image.alpha.as_ref() {
                let planar_yuv = YuvPlanarImageWithAlpha {
                    y_plane: &image.planes[0],
                    y_stride: image.strides[0] as u32,
                    u_plane: &image.planes[1],
                    u_stride: image.strides[1] as u32,
                    v_plane: &image.planes[2],
                    v_stride: image.strides[1] as u32,
                    a_plane: &alpha_plane.data,
                    a_stride: alpha_plane.stride as u32,
                    width: image.width,
                    height: image.height,
                };

                $alpha_to_rgba(
                    &planar_yuv,
                    &mut target_data,
                    image.width * 4,
                    colors.range,
                    colors.matrix,
                    false,
                )
                .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
            }

            let planar_yuv = YuvPlanarImage {
                y_plane: &image.planes[0],
                y_stride: image.strides[0] as u32,
                u_plane: &image.planes[1],
                u_stride: image.strides[1] as u32,
                v_plane: &image.planes[2],
                v_stride: image.strides[1] as u32,
                width: image.width,
                height: image.height,
            };

            $to_rgba(
                &planar_yuv,
                &mut target_data,
                image.width * 4,
                colors.range,
                colors.matrix,
            )
            .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
        }};
    }

    macro_rules! decode_chroma_to_rgba_cgco {
        ($to_rgba:ident, $alpha_to_rgba:ident, $sub_w: expr) => {{
            if let Some(alpha_plane) = image.alpha.as_ref() {
                let planar_yuv = YuvPlanarImageWithAlpha {
                    y_plane: &image.planes[0],
                    y_stride: image.strides[0] as u32,
                    u_plane: &image.planes[1],
                    u_stride: image.strides[1] as u32,
                    v_plane: &image.planes[2],
                    v_stride: image.strides[1] as u32,
                    a_plane: &alpha_plane.data,
                    a_stride: alpha_plane.stride as u32,
                    width: image.width,
                    height: image.height,
                };

                $alpha_to_rgba(&planar_yuv, &mut target_data, image.width * 4, colors.range)
                    .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
            }

            let planar_yuv = YuvPlanarImage {
                y_plane: &image.planes[0],
                y_stride: image.strides[0] as u32,
                u_plane: &image.planes[1],
                u_stride: image.strides[1] as u32,
                v_plane: &image.planes[2],
                v_stride: image.strides[1] as u32,
                width: image.width,
                height: image.height,
            };

            $to_rgba(&planar_yuv, &mut target_data, image.width * 4, colors.range)
                .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
        }};
    }
    match image.pixel_layout {
        tealdust::PixelLayout::I400 => unreachable!("Handled in some other place"),
        tealdust::PixelLayout::I420 => {
            if is_ycgco {
                decode_chroma_to_rgba_cgco!(ycgco420_to_rgba, ycgco420_alpha_to_rgba, 2)
            } else {
                decode_chroma_to_rgba!(yuv420_to_rgba, yuv420_alpha_to_rgba, 2)
            }
        }
        tealdust::PixelLayout::I422 => {
            if is_ycgco {
                decode_chroma_to_rgba_cgco!(ycgco422_to_rgba, ycgco422_alpha_to_rgba, 2)
            } else {
                decode_chroma_to_rgba!(yuv422_to_rgba, yuv422_alpha_to_rgba, 2)
            }
        }
        tealdust::PixelLayout::I444 => {
            if is_ycgco {
                decode_chroma_to_rgba_cgco!(ycgco422_to_rgba, ycgco422_alpha_to_rgba, 2)
            } else {
                decode_chroma_to_rgba!(yuv444_to_rgba, yuv444_alpha_to_rgba, 1)
            }
        }
    }

    finalize(target_data, image, image_info, colors.cicp)
}

#[inline(never)]
fn pack_u8_into_u16(data: &[u8]) -> Vec<u16> {
    data.as_chunks::<2>()
        .0
        .iter()
        .map(|&x| u16::from_ne_bytes([x[0], x[1]]))
        .collect()
}

fn decode_av2_inner_10bit(
    image: &AvifImage,
    image_info: &tealdust::AvifImageInfo,
) -> Result<DecodedAvifPacket<u16>, WeaverError> {
    if check_image_size_overflow(
        image.width as u64,
        image.height as u64,
        4,
        size_of::<u16>() as isize,
    ) {
        return Err(WeaverError::FailedToAllocateMemory(isize::MAX as u64));
    }

    let mut target_data = try_vec![0u16; image.width as usize * image.height as usize * 4];

    let colors = solve_av2_colors(image)?;
    let is_ycgco = is_metadata_ycgco(image);

    if image.pixel_layout == tealdust::PixelLayout::I400 {
        if let Some(alpha_plane) = image.alpha.as_ref() {
            let gray_alpha_img = YuvGrayAlphaImage {
                y_plane: &pack_u8_into_u16(&image.planes[0]),
                y_stride: image.strides[0] as u32,
                a_plane: &pack_u8_into_u16(&alpha_plane.data),
                a_stride: alpha_plane.stride as u32,
                width: image.width,
                height: image.height,
            };
            y010_alpha_to_rgba10(
                &gray_alpha_img,
                &mut target_data,
                image.width * 4,
                colors.range,
                colors.matrix,
            )
            .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;

            return finalize(target_data, image, image_info, colors.cicp);
        }

        let gray_img = YuvGrayImage {
            y_plane: &pack_u8_into_u16(&image.planes[0]),
            y_stride: image.strides[0] as u32,
            width: image.width,
            height: image.height,
        };
        y010_to_rgba10(
            &gray_img,
            &mut target_data,
            image.width * 4,
            colors.range,
            colors.matrix,
        )
        .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;

        return finalize(target_data, image, image_info, colors.cicp);
    }

    macro_rules! decode_chroma_to_rgba {
        ($to_rgba:ident, $alpha_to_rgba:ident, $sub_w: expr) => {{
            if let Some(alpha_plane) = image.alpha.as_ref() {
                let planar_yuv = YuvPlanarImageWithAlpha {
                    y_plane: &pack_u8_into_u16(&image.planes[0]),
                    y_stride: image.strides[0] as u32,
                    u_plane: &pack_u8_into_u16(&image.planes[1]),
                    u_stride: image.strides[1] as u32,
                    v_plane: &pack_u8_into_u16(&image.planes[2]),
                    v_stride: image.strides[1] as u32,
                    a_plane: &pack_u8_into_u16(&alpha_plane.data),
                    a_stride: alpha_plane.stride as u32,
                    width: image.width,
                    height: image.height,
                };

                $alpha_to_rgba(
                    &planar_yuv,
                    &mut target_data,
                    image.width * 4,
                    colors.range,
                    colors.matrix,
                )
                .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
            }

            let planar_yuv = YuvPlanarImage {
                y_plane: &pack_u8_into_u16(&image.planes[0]),
                y_stride: image.strides[0] as u32,
                u_plane: &pack_u8_into_u16(&image.planes[1]),
                u_stride: image.strides[1] as u32,
                v_plane: &pack_u8_into_u16(&image.planes[2]),
                v_stride: image.strides[1] as u32,
                width: image.width,
                height: image.height,
            };

            $to_rgba(
                &planar_yuv,
                &mut target_data,
                image.width * 4,
                colors.range,
                colors.matrix,
            )
            .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
        }};
    }

    macro_rules! decode_chroma_to_rgba_ycgco {
        ($to_rgba:ident, $alpha_to_rgba:ident, $sub_w: expr) => {{
            if let Some(alpha_plane) = image.alpha.as_ref() {
                let planar_yuv = YuvPlanarImageWithAlpha {
                    y_plane: &pack_u8_into_u16(&image.planes[0]),
                    y_stride: image.strides[0] as u32,
                    u_plane: &pack_u8_into_u16(&image.planes[1]),
                    u_stride: image.strides[1] as u32,
                    v_plane: &pack_u8_into_u16(&image.planes[2]),
                    v_stride: image.strides[1] as u32,
                    a_plane: &pack_u8_into_u16(&alpha_plane.data),
                    a_stride: alpha_plane.stride as u32,
                    width: image.width,
                    height: image.height,
                };

                $alpha_to_rgba(&planar_yuv, &mut target_data, image.width * 4, colors.range)
                    .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
            }

            let planar_yuv = YuvPlanarImage {
                y_plane: &pack_u8_into_u16(&image.planes[0]),
                y_stride: image.strides[0] as u32,
                u_plane: &pack_u8_into_u16(&image.planes[1]),
                u_stride: image.strides[1] as u32,
                v_plane: &pack_u8_into_u16(&image.planes[2]),
                v_stride: image.strides[1] as u32,
                width: image.width,
                height: image.height,
            };

            $to_rgba(&planar_yuv, &mut target_data, image.width * 4, colors.range)
                .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
        }};
    }

    match image.pixel_layout {
        tealdust::PixelLayout::I400 => unreachable!("Handled in some other place"),
        tealdust::PixelLayout::I420 => {
            if is_ycgco {
                decode_chroma_to_rgba_ycgco!(icgc010_to_rgba10, icgc010_alpha_to_rgba10, 2)
            } else {
                decode_chroma_to_rgba!(i010_to_rgba10, i010_alpha_to_rgba10, 2)
            }
        }
        tealdust::PixelLayout::I422 => {
            if is_ycgco {
                decode_chroma_to_rgba_ycgco!(icgc210_to_rgba10, icgc210_alpha_to_rgba10, 2)
            } else {
                decode_chroma_to_rgba!(i210_to_rgba10, i210_alpha_to_rgba10, 2)
            }
        }
        tealdust::PixelLayout::I444 => {
            if is_ycgco {
                decode_chroma_to_rgba_ycgco!(icgc412_to_rgba12, icgc412_alpha_to_rgba12, 1)
            } else {
                decode_chroma_to_rgba!(i410_to_rgba10, i410_alpha_to_rgba10, 1)
            }
        }
    }

    finalize(target_data, image, image_info, colors.cicp)
}

fn decode_av2_inner_12bit(
    image: &AvifImage,
    image_info: &tealdust::AvifImageInfo,
) -> Result<DecodedAvifPacket<u16>, WeaverError> {
    if check_image_size_overflow(
        image.width as u64,
        image.height as u64,
        4,
        size_of::<u16>() as isize,
    ) {
        return Err(WeaverError::FailedToAllocateMemory(isize::MAX as u64));
    }

    let mut target_data = try_vec![0u16; image.width as usize * image.height as usize * 4];

    let colors = solve_av2_colors(image)?;
    let is_ycgco = is_metadata_ycgco(image);

    if image.pixel_layout == tealdust::PixelLayout::I400 {
        if let Some(alpha_plane) = image.alpha.as_ref() {
            let gray_alpha_img = YuvGrayAlphaImage {
                y_plane: &pack_u8_into_u16(&image.planes[0]),
                y_stride: image.strides[0] as u32,
                a_plane: &pack_u8_into_u16(&alpha_plane.data),
                a_stride: alpha_plane.stride as u32,
                width: image.width,
                height: image.height,
            };
            y012_alpha_to_rgba12(
                &gray_alpha_img,
                &mut target_data,
                image.width * 4,
                colors.range,
                colors.matrix,
            )
            .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;

            return finalize(target_data, image, image_info, colors.cicp);
        }

        let gray_img = YuvGrayImage {
            y_plane: &pack_u8_into_u16(&image.planes[0]),
            y_stride: image.strides[0] as u32,
            width: image.width,
            height: image.height,
        };
        y012_to_rgba12(
            &gray_img,
            &mut target_data,
            image.width * 4,
            colors.range,
            colors.matrix,
        )
        .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;

        return finalize(target_data, image, image_info, colors.cicp);
    }

    macro_rules! decode_chroma_to_rgba {
        ($to_rgba:ident, $alpha_to_rgba:ident, $sub_w: expr) => {{
            if let Some(alpha_plane) = image.alpha.as_ref() {
                let planar_yuv = YuvPlanarImageWithAlpha {
                    y_plane: &pack_u8_into_u16(&image.planes[0]),
                    y_stride: image.strides[0] as u32,
                    u_plane: &pack_u8_into_u16(&image.planes[1]),
                    u_stride: image.strides[1] as u32,
                    v_plane: &pack_u8_into_u16(&image.planes[2]),
                    v_stride: image.strides[1] as u32,
                    a_plane: &pack_u8_into_u16(&alpha_plane.data),
                    a_stride: alpha_plane.stride as u32,
                    width: image.width,
                    height: image.height,
                };

                $alpha_to_rgba(
                    &planar_yuv,
                    &mut target_data,
                    image.width * 4,
                    colors.range,
                    colors.matrix,
                )
                .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
            }

            let planar_yuv = YuvPlanarImage {
                y_plane: &pack_u8_into_u16(&image.planes[0]),
                y_stride: image.strides[0] as u32,
                u_plane: &pack_u8_into_u16(&image.planes[1]),
                u_stride: image.strides[1] as u32,
                v_plane: &pack_u8_into_u16(&image.planes[2]),
                v_stride: image.strides[1] as u32,
                width: image.width,
                height: image.height,
            };

            $to_rgba(
                &planar_yuv,
                &mut target_data,
                image.width * 4,
                colors.range,
                colors.matrix,
            )
            .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
        }};
    }

    macro_rules! decode_chroma_to_rgba_cgco {
        ($to_rgba:ident, $alpha_to_rgba:ident, $sub_w: expr) => {{
            if let Some(alpha_plane) = image.alpha.as_ref() {
                let planar_yuv = YuvPlanarImageWithAlpha {
                    y_plane: &pack_u8_into_u16(&image.planes[0]),
                    y_stride: image.strides[0] as u32,
                    u_plane: &pack_u8_into_u16(&image.planes[1]),
                    u_stride: image.strides[1] as u32,
                    v_plane: &pack_u8_into_u16(&image.planes[2]),
                    v_stride: image.strides[1] as u32,
                    a_plane: &pack_u8_into_u16(&alpha_plane.data),
                    a_stride: alpha_plane.stride as u32,
                    width: image.width,
                    height: image.height,
                };

                $alpha_to_rgba(&planar_yuv, &mut target_data, image.width * 4, colors.range)
                    .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
            }

            let planar_yuv = YuvPlanarImage {
                y_plane: &pack_u8_into_u16(&image.planes[0]),
                y_stride: image.strides[0] as u32,
                u_plane: &pack_u8_into_u16(&image.planes[1]),
                u_stride: image.strides[1] as u32,
                v_plane: &pack_u8_into_u16(&image.planes[2]),
                v_stride: image.strides[1] as u32,
                width: image.width,
                height: image.height,
            };

            $to_rgba(&planar_yuv, &mut target_data, image.width * 4, colors.range)
                .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
        }};
    }

    match image.pixel_layout {
        tealdust::PixelLayout::I400 => unreachable!("Handled in some other place"),
        tealdust::PixelLayout::I420 => {
            if is_ycgco {
                decode_chroma_to_rgba_cgco!(icgc012_to_rgba12, icgc012_alpha_to_rgba12, 2)
            } else {
                decode_chroma_to_rgba!(i012_to_rgba12, i012_alpha_to_rgba12, 2)
            }
        }
        tealdust::PixelLayout::I422 => {
            if is_ycgco {
                decode_chroma_to_rgba_cgco!(icgc212_to_rgba12, icgc212_alpha_to_rgba12, 2)
            } else {
                decode_chroma_to_rgba!(i212_to_rgba12, i212_alpha_to_rgba12, 2)
            }
        }
        tealdust::PixelLayout::I444 => {
            if is_ycgco {
                decode_chroma_to_rgba_cgco!(icgc412_to_rgba12, icgc412_alpha_to_rgba12, 1)
            } else {
                decode_chroma_to_rgba!(i412_to_rgba12, i412_alpha_to_rgba12, 1)
            }
        }
    }

    finalize(target_data, image, image_info, colors.cicp)
}

/// # Arguments
///
/// * `data`:
///
/// returns: Result<PackedHeic, WeaverError>
pub(crate) fn decode_packed_av2(data: &[u8]) -> Result<PackedAv2, WeaverError> {
    let mut decoder = tealdust::AvifDecoder::new(data)
        .map_err(|x| WeaverError::FailedToDecodeAv2(x.to_string()))?;
    let image_info = decoder
        .image_info()
        .map_err(|x| WeaverError::FailedToDecodeAv2(x.to_string()))?;
    let image = decoder
        .decode()
        .map_err(|x| WeaverError::FailedToDecodeAv2(x.to_string()))?;
    match image_info.bits_per_component {
        8 => Ok(PackedAv2::Regular(decode_inner_low_bit_depth(
            &image,
            &image_info,
        )?)),
        10 => Ok(PackedAv2::HighBitDepth(decode_av2_inner_10bit(
            &image,
            &image_info,
        )?)),
        12 => Ok(PackedAv2::HighBitDepth(decode_av2_inner_12bit(
            &image,
            &image_info,
        )?)),
        _ => Err(WeaverError::FailedToDecodeAv2(
            format!(
                "AV2 unsupported image bit depth {}",
                image_info.bits_per_component
            )
            .to_string(),
        )),
    }
}
