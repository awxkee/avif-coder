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
use crate::orientation::apply_orientation;
use crate::support::try_vec;
use hpvcd::{
    BitDepth, Cicp, DecodedYuv, MatrixCoefficients, Orientation, Primaries, TransferFunction,
};
use thiserror::Error;
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

fn is_heic(bytes: &[u8]) -> bool {
    if bytes.len() < 12 {
        return false;
    }
    if &bytes[4..8] != b"ftyp" {
        return false;
    }
    matches!(
        &bytes[8..12],
        b"heic" | b"heix" | b"hevc" | b"hevx" | b"mif1" | b"msf1" | b"miaf" | b"MiHE"
    )
}

#[derive(Debug, Error)]
pub enum WeaverError {
    #[error("Data is not heic file")]
    InvalidHeic,
    #[error("HEIC decoder failed with an errror {0}")]
    FailedToDecodeHeic(String),
    #[error("AVIF AV2 decoder failed with an errror {0}")]
    FailedToDecodeAv2(String),
    #[error("Unsupported matrix coefficients {0:?}")]
    UnsupportedMatrix(MatrixCoefficients),
    #[error("Unsupported AV2 matrix coefficients {0:?}")]
    UnsupportedMatrixAv2(tealdust::MatrixCoefficients),
    #[error("Depth signalled for encoded plane doesn't match the container")]
    MismatchedBitDepth,
    #[error("Failed to allocate memory with size {0}")]
    FailedToAllocateMemory(u64),
    #[error("YUV decoding failed with an error {0}")]
    YuvDecodingSignalledError(String),
    #[error("YUV decoding failed with an error {0}")]
    PixelFormatIsNotSupported(String),
    #[error("Monochrome in current path is not supported")]
    MonochromeIsNotSupported,
}

pub(crate) struct DecodedHeicPacket<T> {
    pub(crate) data: Vec<T>,
    pub(crate) width: usize,
    pub(crate) height: usize,
    pub(crate) colors: Cicp,
    pub(crate) icc: Option<Vec<u8>>,
    pub(crate) bit_depth: BitDepth,
    pub(crate) has_real_alpha: bool,
}

fn clap_rect(
    width: usize,
    height: usize,
    clap: &hpvcd::CleanAperture,
) -> Option<(usize, usize, usize, usize)> {
    if clap.width_d == 0 || clap.height_d == 0 || clap.horiz_off_d == 0 || clap.vert_off_d == 0 {
        return None;
    }
    let (img_w, img_h) = (width as i64, height as i64);

    let crop_w = ((clap.width_n as i64) / (clap.width_d as i64)).min(img_w);
    let crop_h = ((clap.height_n as i64) / (clap.height_d as i64)).min(img_h);
    if crop_w <= 0 || crop_h <= 0 {
        return None;
    }

    // ISO 14496-12: offsets locate the aperture *center* relative to image center.
    //   left = horizOff + (W-1)/2 - (cropW-1)/2   (evaluated over denom 2*off_d)
    let hd = clap.horiz_off_d as i64;
    let vd = clap.vert_off_d as i64;
    let left = (2 * (clap.horiz_off_n as i64) + hd * (img_w - 1) - hd * (crop_w - 1))
        .div_euclid(2 * hd)
        .clamp(0, img_w - crop_w);
    let top = (2 * (clap.vert_off_n as i64) + vd * (img_h - 1) - vd * (crop_h - 1))
        .div_euclid(2 * vd)
        .clamp(0, img_h - crop_h);

    Some((
        left as usize,
        top as usize,
        crop_w as usize,
        crop_h as usize,
    ))
}

fn finalize<T: Copy + Default>(
    mut data: Vec<T>,
    decoded_yuv: &DecodedYuv,
    cicp: Cicp,
) -> Result<DecodedHeicPacket<T>, WeaverError> {
    const CH: usize = 4; // RGBA
    let mut width = decoded_yuv.width as usize;
    let mut height = decoded_yuv.height as usize;

    // clap is defined in coded space, so crop BEFORE orientation (clap → irot → imir).
    if let Some(clap) = decoded_yuv.clean_aperture.as_ref()
        && let Some((left, top, cw, ch)) = clap_rect(width, height, clap)
        && (cw != width || ch != height)
    {
        let (src_stride, dst_stride) = (width * CH, cw * CH);
        let mut cropped = try_vec![T::default(); cw * ch * CH];
        for row in 0..ch {
            let s = (top + row) * src_stride + left * CH;
            let d = row * dst_stride;
            cropped[d..d + dst_stride].copy_from_slice(&data[s..s + dst_stride]);
        }
        data = cropped;
        width = cw;
        height = ch;
    }

    if decoded_yuv.orientation != Orientation::Normal {
        let (nd, nw, nh) = apply_orientation(&data, width, height, decoded_yuv.orientation);
        data = nd;
        width = nw;
        height = nh;
    }

    Ok(DecodedHeicPacket {
        data,
        width,
        height,
        colors: cicp,
        icc: decoded_yuv.color.icc.as_ref().map(|x| x.to_vec()),
        bit_depth: decoded_yuv.bit_depth,
        has_real_alpha: decoded_yuv.alpha.is_some(),
    })
}

pub(crate) enum PackedHeic {
    Regular(DecodedHeicPacket<u8>),
    HighBitDepth(DecodedHeicPacket<u16>),
}

struct HeicColors {
    cicp: Cicp,
    matrix: YuvStandardMatrix,
    range: yuv::YuvRange,
}

fn solve_heic_colors(decoded_yuv: &DecodedYuv) -> Result<HeicColors, WeaverError> {
    let cicp = decoded_yuv.color.cicp.unwrap_or(Cicp {
        primaries: Primaries::Bt709,
        transfer: TransferFunction::Srgb,
        matrix: MatrixCoefficients::Smpte170m,
        full_range: true,
    });
    let yuv_range = match cicp.full_range {
        true => yuv::YuvRange::Limited,
        false => yuv::YuvRange::Full,
    };

    let matrix: YuvStandardMatrix = match cicp.matrix {
        MatrixCoefficients::Bt709 => YuvStandardMatrix::Bt709,
        MatrixCoefficients::Unspecified => YuvStandardMatrix::Bt601,
        MatrixCoefficients::Smpte170m => YuvStandardMatrix::Bt601,
        MatrixCoefficients::Bt2020Ncl => YuvStandardMatrix::Bt2020,
        MatrixCoefficients::Fcc => YuvStandardMatrix::Fcc,
        MatrixCoefficients::YCgCo => YuvStandardMatrix::Custom(0.25, 0.25),
        v => return Err(WeaverError::UnsupportedMatrix(v)),
    };
    Ok(HeicColors {
        cicp,
        range: yuv_range,
        matrix,
    })
}

fn is_metadata_ycgco(decoded_yuv: &DecodedYuv) -> bool {
    decoded_yuv
        .color
        .cicp
        .unwrap_or(Cicp {
            primaries: Primaries::Bt709,
            transfer: TransferFunction::Srgb,
            matrix: MatrixCoefficients::Smpte170m,
            full_range: true,
        })
        .matrix
        == MatrixCoefficients::YCgCo
}

fn decode_inner_low_bit_depth(
    decoded_yuv: &DecodedYuv,
) -> Result<DecodedHeicPacket<u8>, WeaverError> {
    if check_image_size_overflow(
        decoded_yuv.width as u64,
        decoded_yuv.height as u64,
        4,
        size_of::<u8>() as isize,
    ) {
        return Err(WeaverError::FailedToAllocateMemory(isize::MAX as u64));
    }

    let mut target_data =
        try_vec![0u8; decoded_yuv.width as usize * decoded_yuv.height as usize * 4];

    let colors = solve_heic_colors(decoded_yuv)?;
    let is_ycgco = is_metadata_ycgco(decoded_yuv);

    if decoded_yuv.chroma == hpvcd::ChromaFormat::Monochrome {
        let probably_u8_gray = decoded_yuv.y.as_u8();
        let Some(gray_plane) = probably_u8_gray.as_ref() else {
            return Err(WeaverError::MismatchedBitDepth);
        };

        if let Some(hidden_alpha) = decoded_yuv.alpha.as_ref() {
            let bound_alpha_u8 = hidden_alpha.as_u8();
            let Some(alpha_plane) = bound_alpha_u8.as_ref() else {
                return Err(WeaverError::MismatchedBitDepth);
            };

            let gray_alpha_img = YuvGrayAlphaImage {
                y_plane: gray_plane,
                y_stride: decoded_yuv.width,
                a_plane: alpha_plane,
                a_stride: decoded_yuv.width,
                width: decoded_yuv.width,
                height: decoded_yuv.height,
            };
            yuv400_alpha_to_rgba(
                &gray_alpha_img,
                &mut target_data,
                decoded_yuv.width * 4,
                colors.range,
                colors.matrix,
            )
            .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;

            return finalize(target_data, decoded_yuv, colors.cicp);
        }

        let gray_img = YuvGrayImage {
            y_plane: gray_plane,
            y_stride: decoded_yuv.width,
            width: decoded_yuv.width,
            height: decoded_yuv.height,
        };
        yuv400_to_rgba(
            &gray_img,
            &mut target_data,
            decoded_yuv.width * 4,
            colors.range,
            colors.matrix,
        )
        .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;

        return finalize(target_data, decoded_yuv, colors.cicp);
    }

    let probably_u8_luma = decoded_yuv.y.as_u8();
    let Some(luma_plane) = probably_u8_luma.as_ref() else {
        return Err(WeaverError::MismatchedBitDepth);
    };
    let probably_cb_plane = decoded_yuv.cb.as_u8();
    let Some(cb_plane) = probably_cb_plane.as_ref() else {
        return Err(WeaverError::MismatchedBitDepth);
    };
    let probably_cr_plane = decoded_yuv.cr.as_u8();
    let Some(cr_plane) = probably_cr_plane.as_ref() else {
        return Err(WeaverError::MismatchedBitDepth);
    };

    macro_rules! decode_chroma_to_rgba {
        ($to_rgba:ident, $alpha_to_rgba:ident, $sub_w: expr) => {{
            if let Some(hidden_alpha) = decoded_yuv.alpha.as_ref() {
                let bound_alpha_u8 = hidden_alpha.as_u8();
                let Some(alpha_plane) = bound_alpha_u8.as_ref() else {
                    return Err(WeaverError::MismatchedBitDepth);
                };

                let planar_yuv = YuvPlanarImageWithAlpha {
                    y_plane: luma_plane,
                    y_stride: decoded_yuv.width,
                    u_plane: cb_plane,
                    u_stride: if $sub_w != 1 {
                        decoded_yuv.width.div_ceil($sub_w)
                    } else {
                        decoded_yuv.width
                    },
                    v_plane: cr_plane,
                    v_stride: if $sub_w != 1 {
                        decoded_yuv.width.div_ceil($sub_w)
                    } else {
                        decoded_yuv.width
                    },
                    a_plane: alpha_plane,
                    a_stride: decoded_yuv.width,
                    width: decoded_yuv.width,
                    height: decoded_yuv.height,
                };

                $alpha_to_rgba(
                    &planar_yuv,
                    &mut target_data,
                    decoded_yuv.width * 4,
                    colors.range,
                    colors.matrix,
                    false,
                )
                .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
            }

            let planar_yuv = YuvPlanarImage {
                y_plane: luma_plane,
                y_stride: decoded_yuv.width,
                u_plane: cb_plane,
                u_stride: if $sub_w != 1 {
                    decoded_yuv.width.div_ceil($sub_w)
                } else {
                    decoded_yuv.width
                },
                v_plane: cr_plane,
                v_stride: if $sub_w != 1 {
                    decoded_yuv.width.div_ceil($sub_w)
                } else {
                    decoded_yuv.width
                },
                width: decoded_yuv.width,
                height: decoded_yuv.height,
            };

            $to_rgba(
                &planar_yuv,
                &mut target_data,
                decoded_yuv.width * 4,
                colors.range,
                colors.matrix,
            )
            .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
        }};
    }

    macro_rules! decode_chroma_to_rgba_cgco {
        ($to_rgba:ident, $alpha_to_rgba:ident, $sub_w: expr) => {{
            if let Some(hidden_alpha) = decoded_yuv.alpha.as_ref() {
                let bound_alpha_u8 = hidden_alpha.as_u8();
                let Some(alpha_plane) = bound_alpha_u8.as_ref() else {
                    return Err(WeaverError::MismatchedBitDepth);
                };

                let planar_yuv = YuvPlanarImageWithAlpha {
                    y_plane: luma_plane,
                    y_stride: decoded_yuv.width,
                    u_plane: cb_plane,
                    u_stride: if $sub_w != 1 {
                        decoded_yuv.width.div_ceil($sub_w)
                    } else {
                        decoded_yuv.width
                    },
                    v_plane: cr_plane,
                    v_stride: if $sub_w != 1 {
                        decoded_yuv.width.div_ceil($sub_w)
                    } else {
                        decoded_yuv.width
                    },
                    a_plane: alpha_plane,
                    a_stride: decoded_yuv.width,
                    width: decoded_yuv.width,
                    height: decoded_yuv.height,
                };

                $alpha_to_rgba(
                    &planar_yuv,
                    &mut target_data,
                    decoded_yuv.width * 4,
                    colors.range,
                )
                .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
            }

            let planar_yuv = YuvPlanarImage {
                y_plane: luma_plane,
                y_stride: decoded_yuv.width,
                u_plane: cb_plane,
                u_stride: if $sub_w != 1 {
                    decoded_yuv.width.div_ceil($sub_w)
                } else {
                    decoded_yuv.width
                },
                v_plane: cr_plane,
                v_stride: if $sub_w != 1 {
                    decoded_yuv.width.div_ceil($sub_w)
                } else {
                    decoded_yuv.width
                },
                width: decoded_yuv.width,
                height: decoded_yuv.height,
            };

            $to_rgba(
                &planar_yuv,
                &mut target_data,
                decoded_yuv.width * 4,
                colors.range,
            )
            .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
        }};
    }
    match decoded_yuv.chroma {
        hpvcd::ChromaFormat::Monochrome => unreachable!("Handled in some other place"),
        hpvcd::ChromaFormat::Yuv420 => {
            if is_ycgco {
                decode_chroma_to_rgba_cgco!(ycgco420_to_rgba, ycgco420_alpha_to_rgba, 2)
            } else {
                decode_chroma_to_rgba!(yuv420_to_rgba, yuv420_alpha_to_rgba, 2)
            }
        }
        hpvcd::ChromaFormat::Yuv422 => {
            if is_ycgco {
                decode_chroma_to_rgba_cgco!(ycgco422_to_rgba, ycgco422_alpha_to_rgba, 2)
            } else {
                decode_chroma_to_rgba!(yuv422_to_rgba, yuv422_alpha_to_rgba, 2)
            }
        }
        hpvcd::ChromaFormat::Yuv444 => {
            if is_ycgco {
                decode_chroma_to_rgba_cgco!(ycgco422_to_rgba, ycgco422_alpha_to_rgba, 2)
            } else {
                decode_chroma_to_rgba!(yuv444_to_rgba, yuv444_alpha_to_rgba, 1)
            }
        }
    }

    finalize(target_data, decoded_yuv, colors.cicp)
}

fn decode_heic_inner_10bit(
    decoded_yuv: &DecodedYuv,
) -> Result<DecodedHeicPacket<u16>, WeaverError> {
    if check_image_size_overflow(
        decoded_yuv.width as u64,
        decoded_yuv.height as u64,
        4,
        size_of::<u16>() as isize,
    ) {
        return Err(WeaverError::FailedToAllocateMemory(isize::MAX as u64));
    }

    let mut target_data =
        try_vec![0u16; decoded_yuv.width as usize * decoded_yuv.height as usize * 4];

    let colors = solve_heic_colors(decoded_yuv)?;
    let is_ycgco = is_metadata_ycgco(decoded_yuv);

    if decoded_yuv.chroma == hpvcd::ChromaFormat::Monochrome {
        let probably_u16_gray = decoded_yuv.y.as_u16();
        let Some(gray_plane) = probably_u16_gray.as_ref() else {
            return Err(WeaverError::MismatchedBitDepth);
        };

        if let Some(hidden_alpha) = decoded_yuv.alpha.as_ref() {
            let bound_alpha_u16 = hidden_alpha.as_u16();
            let Some(alpha_plane) = bound_alpha_u16.as_ref() else {
                return Err(WeaverError::MismatchedBitDepth);
            };

            let gray_alpha_img = YuvGrayAlphaImage {
                y_plane: gray_plane,
                y_stride: decoded_yuv.width,
                a_plane: alpha_plane,
                a_stride: decoded_yuv.width,
                width: decoded_yuv.width,
                height: decoded_yuv.height,
            };
            y010_alpha_to_rgba10(
                &gray_alpha_img,
                &mut target_data,
                decoded_yuv.width * 4,
                colors.range,
                colors.matrix,
            )
            .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;

            return finalize(target_data, decoded_yuv, colors.cicp);
        }

        let gray_img = YuvGrayImage {
            y_plane: gray_plane,
            y_stride: decoded_yuv.width,
            width: decoded_yuv.width,
            height: decoded_yuv.height,
        };
        y010_to_rgba10(
            &gray_img,
            &mut target_data,
            decoded_yuv.width * 4,
            colors.range,
            colors.matrix,
        )
        .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;

        return finalize(target_data, decoded_yuv, colors.cicp);
    }

    let probably_u16_luma = decoded_yuv.y.as_u16();
    let Some(luma_plane) = probably_u16_luma.as_ref() else {
        return Err(WeaverError::MismatchedBitDepth);
    };
    let probably_cb_plane = decoded_yuv.cb.as_u16();
    let Some(cb_plane) = probably_cb_plane.as_ref() else {
        return Err(WeaverError::MismatchedBitDepth);
    };
    let probably_cr_plane = decoded_yuv.cr.as_u16();
    let Some(cr_plane) = probably_cr_plane.as_ref() else {
        return Err(WeaverError::MismatchedBitDepth);
    };

    macro_rules! decode_chroma_to_rgba {
        ($to_rgba:ident, $alpha_to_rgba:ident, $sub_w: expr) => {{
            if let Some(hidden_alpha) = decoded_yuv.alpha.as_ref() {
                let bound_alpha_u16 = hidden_alpha.as_u16();
                let Some(alpha_plane) = bound_alpha_u16.as_ref() else {
                    return Err(WeaverError::MismatchedBitDepth);
                };

                let planar_yuv = YuvPlanarImageWithAlpha {
                    y_plane: luma_plane,
                    y_stride: decoded_yuv.width,
                    u_plane: cb_plane,
                    u_stride: if $sub_w != 1 {
                        decoded_yuv.width.div_ceil($sub_w)
                    } else {
                        decoded_yuv.width
                    },
                    v_plane: cr_plane,
                    v_stride: if $sub_w != 1 {
                        decoded_yuv.width.div_ceil($sub_w)
                    } else {
                        decoded_yuv.width
                    },
                    a_plane: alpha_plane,
                    a_stride: decoded_yuv.width,
                    width: decoded_yuv.width,
                    height: decoded_yuv.height,
                };

                $alpha_to_rgba(
                    &planar_yuv,
                    &mut target_data,
                    decoded_yuv.width * 4,
                    colors.range,
                    colors.matrix,
                )
                .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
            }

            let planar_yuv = YuvPlanarImage {
                y_plane: luma_plane,
                y_stride: decoded_yuv.width,
                u_plane: cb_plane,
                u_stride: if $sub_w != 1 {
                    decoded_yuv.width.div_ceil($sub_w)
                } else {
                    decoded_yuv.width
                },
                v_plane: cr_plane,
                v_stride: if $sub_w != 1 {
                    decoded_yuv.width.div_ceil($sub_w)
                } else {
                    decoded_yuv.width
                },
                width: decoded_yuv.width,
                height: decoded_yuv.height,
            };

            $to_rgba(
                &planar_yuv,
                &mut target_data,
                decoded_yuv.width * 4,
                colors.range,
                colors.matrix,
            )
            .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
        }};
    }

    macro_rules! decode_chroma_to_rgba_ycgco {
        ($to_rgba:ident, $alpha_to_rgba:ident, $sub_w: expr) => {{
            if let Some(hidden_alpha) = decoded_yuv.alpha.as_ref() {
                let bound_alpha_u16 = hidden_alpha.as_u16();
                let Some(alpha_plane) = bound_alpha_u16.as_ref() else {
                    return Err(WeaverError::MismatchedBitDepth);
                };

                let planar_yuv = YuvPlanarImageWithAlpha {
                    y_plane: luma_plane,
                    y_stride: decoded_yuv.width,
                    u_plane: cb_plane,
                    u_stride: if $sub_w != 1 {
                        decoded_yuv.width.div_ceil($sub_w)
                    } else {
                        decoded_yuv.width
                    },
                    v_plane: cr_plane,
                    v_stride: if $sub_w != 1 {
                        decoded_yuv.width.div_ceil($sub_w)
                    } else {
                        decoded_yuv.width
                    },
                    a_plane: alpha_plane,
                    a_stride: decoded_yuv.width,
                    width: decoded_yuv.width,
                    height: decoded_yuv.height,
                };

                $alpha_to_rgba(
                    &planar_yuv,
                    &mut target_data,
                    decoded_yuv.width * 4,
                    colors.range,
                )
                .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
            }

            let planar_yuv = YuvPlanarImage {
                y_plane: luma_plane,
                y_stride: decoded_yuv.width,
                u_plane: cb_plane,
                u_stride: if $sub_w != 1 {
                    decoded_yuv.width.div_ceil($sub_w)
                } else {
                    decoded_yuv.width
                },
                v_plane: cr_plane,
                v_stride: if $sub_w != 1 {
                    decoded_yuv.width.div_ceil($sub_w)
                } else {
                    decoded_yuv.width
                },
                width: decoded_yuv.width,
                height: decoded_yuv.height,
            };

            $to_rgba(
                &planar_yuv,
                &mut target_data,
                decoded_yuv.width * 4,
                colors.range,
            )
            .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
        }};
    }

    match decoded_yuv.chroma {
        hpvcd::ChromaFormat::Monochrome => unreachable!("Handled in some other place"),
        hpvcd::ChromaFormat::Yuv420 => {
            if is_ycgco {
                decode_chroma_to_rgba_ycgco!(icgc010_to_rgba10, icgc010_alpha_to_rgba10, 2)
            } else {
                decode_chroma_to_rgba!(i010_to_rgba10, i010_alpha_to_rgba10, 2)
            }
        }
        hpvcd::ChromaFormat::Yuv422 => {
            if is_ycgco {
                decode_chroma_to_rgba_ycgco!(icgc210_to_rgba10, icgc210_alpha_to_rgba10, 2)
            } else {
                decode_chroma_to_rgba!(i210_to_rgba10, i210_alpha_to_rgba10, 2)
            }
        }
        hpvcd::ChromaFormat::Yuv444 => {
            if is_ycgco {
                decode_chroma_to_rgba_ycgco!(icgc412_to_rgba12, icgc412_alpha_to_rgba12, 1)
            } else {
                decode_chroma_to_rgba!(i410_to_rgba10, i410_alpha_to_rgba10, 1)
            }
        }
    }

    finalize(target_data, decoded_yuv, colors.cicp)
}

fn decode_heic_inner_12bit(
    decoded_yuv: &DecodedYuv,
) -> Result<DecodedHeicPacket<u16>, WeaverError> {
    if check_image_size_overflow(
        decoded_yuv.width as u64,
        decoded_yuv.height as u64,
        4,
        size_of::<u16>() as isize,
    ) {
        return Err(WeaverError::FailedToAllocateMemory(isize::MAX as u64));
    }

    let mut target_data =
        try_vec![0u16; decoded_yuv.width as usize * decoded_yuv.height as usize * 4];

    let colors = solve_heic_colors(decoded_yuv)?;
    let is_ycgco = is_metadata_ycgco(decoded_yuv);

    if decoded_yuv.chroma == hpvcd::ChromaFormat::Monochrome {
        let probably_u16_gray = decoded_yuv.y.as_u16();
        let Some(gray_plane) = probably_u16_gray.as_ref() else {
            return Err(WeaverError::MismatchedBitDepth);
        };

        if let Some(hidden_alpha) = decoded_yuv.alpha.as_ref() {
            let bound_alpha_u16 = hidden_alpha.as_u16();
            let Some(alpha_plane) = bound_alpha_u16.as_ref() else {
                return Err(WeaverError::MismatchedBitDepth);
            };

            let gray_alpha_img = YuvGrayAlphaImage {
                y_plane: gray_plane,
                y_stride: decoded_yuv.width,
                a_plane: alpha_plane,
                a_stride: decoded_yuv.width,
                width: decoded_yuv.width,
                height: decoded_yuv.height,
            };
            y012_alpha_to_rgba12(
                &gray_alpha_img,
                &mut target_data,
                decoded_yuv.width * 4,
                colors.range,
                colors.matrix,
            )
            .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;

            return finalize(target_data, decoded_yuv, colors.cicp);
        }

        let gray_img = YuvGrayImage {
            y_plane: gray_plane,
            y_stride: decoded_yuv.width,
            width: decoded_yuv.width,
            height: decoded_yuv.height,
        };
        y012_to_rgba12(
            &gray_img,
            &mut target_data,
            decoded_yuv.width * 4,
            colors.range,
            colors.matrix,
        )
        .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;

        return finalize(target_data, decoded_yuv, colors.cicp);
    }

    let probably_u16_luma = decoded_yuv.y.as_u16();
    let Some(luma_plane) = probably_u16_luma.as_ref() else {
        return Err(WeaverError::MismatchedBitDepth);
    };
    let probably_cb_plane = decoded_yuv.cb.as_u16();
    let Some(cb_plane) = probably_cb_plane.as_ref() else {
        return Err(WeaverError::MismatchedBitDepth);
    };
    let probably_cr_plane = decoded_yuv.cr.as_u16();
    let Some(cr_plane) = probably_cr_plane.as_ref() else {
        return Err(WeaverError::MismatchedBitDepth);
    };

    macro_rules! decode_chroma_to_rgba {
        ($to_rgba:ident, $alpha_to_rgba:ident, $sub_w: expr) => {{
            if let Some(hidden_alpha) = decoded_yuv.alpha.as_ref() {
                let bound_alpha_u16 = hidden_alpha.as_u16();
                let Some(alpha_plane) = bound_alpha_u16.as_ref() else {
                    return Err(WeaverError::MismatchedBitDepth);
                };

                let planar_yuv = YuvPlanarImageWithAlpha {
                    y_plane: luma_plane,
                    y_stride: decoded_yuv.width,
                    u_plane: cb_plane,
                    u_stride: if $sub_w != 1 {
                        decoded_yuv.width.div_ceil($sub_w)
                    } else {
                        decoded_yuv.width
                    },
                    v_plane: cr_plane,
                    v_stride: if $sub_w != 1 {
                        decoded_yuv.width.div_ceil($sub_w)
                    } else {
                        decoded_yuv.width
                    },
                    a_plane: alpha_plane,
                    a_stride: decoded_yuv.width,
                    width: decoded_yuv.width,
                    height: decoded_yuv.height,
                };

                $alpha_to_rgba(
                    &planar_yuv,
                    &mut target_data,
                    decoded_yuv.width * 4,
                    colors.range,
                    colors.matrix,
                )
                .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
            }

            let planar_yuv = YuvPlanarImage {
                y_plane: luma_plane,
                y_stride: decoded_yuv.width,
                u_plane: cb_plane,
                u_stride: if $sub_w != 1 {
                    decoded_yuv.width.div_ceil($sub_w)
                } else {
                    decoded_yuv.width
                },
                v_plane: cr_plane,
                v_stride: if $sub_w != 1 {
                    decoded_yuv.width.div_ceil($sub_w)
                } else {
                    decoded_yuv.width
                },
                width: decoded_yuv.width,
                height: decoded_yuv.height,
            };

            $to_rgba(
                &planar_yuv,
                &mut target_data,
                decoded_yuv.width * 4,
                colors.range,
                colors.matrix,
            )
            .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
        }};
    }

    macro_rules! decode_chroma_to_rgba_cgco {
        ($to_rgba:ident, $alpha_to_rgba:ident, $sub_w: expr) => {{
            if let Some(hidden_alpha) = decoded_yuv.alpha.as_ref() {
                let bound_alpha_u16 = hidden_alpha.as_u16();
                let Some(alpha_plane) = bound_alpha_u16.as_ref() else {
                    return Err(WeaverError::MismatchedBitDepth);
                };

                let planar_yuv = YuvPlanarImageWithAlpha {
                    y_plane: luma_plane,
                    y_stride: decoded_yuv.width,
                    u_plane: cb_plane,
                    u_stride: if $sub_w != 1 {
                        decoded_yuv.width.div_ceil($sub_w)
                    } else {
                        decoded_yuv.width
                    },
                    v_plane: cr_plane,
                    v_stride: if $sub_w != 1 {
                        decoded_yuv.width.div_ceil($sub_w)
                    } else {
                        decoded_yuv.width
                    },
                    a_plane: alpha_plane,
                    a_stride: decoded_yuv.width,
                    width: decoded_yuv.width,
                    height: decoded_yuv.height,
                };

                $alpha_to_rgba(
                    &planar_yuv,
                    &mut target_data,
                    decoded_yuv.width * 4,
                    colors.range,
                )
                .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
            }

            let planar_yuv = YuvPlanarImage {
                y_plane: luma_plane,
                y_stride: decoded_yuv.width,
                u_plane: cb_plane,
                u_stride: if $sub_w != 1 {
                    decoded_yuv.width.div_ceil($sub_w)
                } else {
                    decoded_yuv.width
                },
                v_plane: cr_plane,
                v_stride: if $sub_w != 1 {
                    decoded_yuv.width.div_ceil($sub_w)
                } else {
                    decoded_yuv.width
                },
                width: decoded_yuv.width,
                height: decoded_yuv.height,
            };

            $to_rgba(
                &planar_yuv,
                &mut target_data,
                decoded_yuv.width * 4,
                colors.range,
            )
            .map_err(|x| WeaverError::YuvDecodingSignalledError(x.to_string()))?;
        }};
    }

    match decoded_yuv.chroma {
        hpvcd::ChromaFormat::Monochrome => unreachable!("Handled in some other place"),
        hpvcd::ChromaFormat::Yuv420 => {
            if is_ycgco {
                decode_chroma_to_rgba_cgco!(icgc012_to_rgba12, icgc012_alpha_to_rgba12, 2)
            } else {
                decode_chroma_to_rgba!(i012_to_rgba12, i012_alpha_to_rgba12, 2)
            }
        }
        hpvcd::ChromaFormat::Yuv422 => {
            if is_ycgco {
                decode_chroma_to_rgba_cgco!(icgc212_to_rgba12, icgc212_alpha_to_rgba12, 2)
            } else {
                decode_chroma_to_rgba!(i212_to_rgba12, i212_alpha_to_rgba12, 2)
            }
        }
        hpvcd::ChromaFormat::Yuv444 => {
            if is_ycgco {
                decode_chroma_to_rgba_cgco!(icgc412_to_rgba12, icgc412_alpha_to_rgba12, 1)
            } else {
                decode_chroma_to_rgba!(i412_to_rgba12, i412_alpha_to_rgba12, 1)
            }
        }
    }

    finalize(target_data, decoded_yuv, colors.cicp)
}

pub(crate) fn decode_packed_heic(data: &[u8]) -> Result<PackedHeic, WeaverError> {
    if !is_heic(data) {
        return Err(WeaverError::InvalidHeic);
    }

    let decoded_heic =
        hpvcd::decode_heic_yuv(data).map_err(|x| WeaverError::FailedToDecodeHeic(x.to_string()))?;

    match decoded_heic.bit_depth {
        BitDepth::Eight => Ok(PackedHeic::Regular(decode_inner_low_bit_depth(
            &decoded_heic,
        )?)),
        BitDepth::Ten => Ok(PackedHeic::HighBitDepth(decode_heic_inner_10bit(
            &decoded_heic,
        )?)),
        BitDepth::Twelve => Ok(PackedHeic::HighBitDepth(decode_heic_inner_12bit(
            &decoded_heic,
        )?)),
    }
}
