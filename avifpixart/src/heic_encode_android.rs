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
use crate::cvt::{ar30_bytes_to_rgba10, f16_bytes_to_rgba10, rgb565_bytes_to_rgba8888};
use crate::ffi::{BitmapData, BitmapPixelFormat, get_bitmap_data};
use crate::heic_decode::WeaverError;
use crate::support::{dbg_log, has_non_constant_alpha, init_logging, optional_bytebuffer_to_vec};
use hpvca::{BitDepth, ChromaFormat, Cicp, MatrixCoefficients, Primaries, TransferFunction};
use jni::objects::JObject;
use jni::strings::JNIString;
use jni::sys::{jbyteArray, jobject};
use jni::{EnvUnowned, Outcome, jni_str};
use ndk_sys::ADataSpace;
use std::num::NonZero;
use std::ptr::null_mut;
use std::thread::available_parallelism;
use yuv::{
    YuvChromaSubsampling, YuvConversionMode, YuvPlanarImageMut, YuvRange, YuvStandardMatrix,
    rgba_to_ycgco420, rgba_to_ycgco422, rgba_to_ycgco444, rgba_to_yuv420, rgba_to_yuv422,
    rgba_to_yuv444, rgba10_to_i010, rgba10_to_i210, rgba10_to_i410, rgba10_to_icgc010,
    rgba10_to_icgc210, rgba10_to_icgc410,
};

fn resolve_cicp(color_space: i32) -> Cicp {
    let ds = ADataSpace(color_space);

    if ds == ADataSpace::ADATASPACE_UNKNOWN {
        return Cicp {
            primaries: Primaries::Bt601,
            transfer: TransferFunction::Srgb,
            matrix: MatrixCoefficients::Smpte170m,
            full_range: false,
        };
    }

    if ds == ADataSpace::ADATASPACE_SCRGB {
        return Cicp {
            primaries: Primaries::Bt601,
            transfer: TransferFunction::Srgb,
            matrix: MatrixCoefficients::Smpte170m,
            full_range: true,
        };
    }

    if ds == ADataSpace::ADATASPACE_BT601_525 || ds == ADataSpace::ADATASPACE_BT601_625 {
        return Cicp {
            primaries: Primaries::Bt601,
            transfer: TransferFunction::Bt601,
            matrix: MatrixCoefficients::Smpte170m,
            full_range: false,
        };
    }

    // BT.709 video (limited range)
    if ds == ADataSpace::ADATASPACE_BT709 {
        return Cicp {
            primaries: Primaries::Bt709,
            transfer: TransferFunction::Bt709,
            matrix: MatrixCoefficients::Bt709,
            full_range: false,
        };
    }

    // sRGB — BT.709 primaries + matrix, sRGB transfer, full range
    if ds == ADataSpace::ADATASPACE_SRGB {
        return Cicp {
            primaries: Primaries::Bt709,
            transfer: TransferFunction::Srgb,
            matrix: MatrixCoefficients::Bt709,
            full_range: true,
        };
    }

    // Linear sRGB (scRGB linear) — BT.709 primaries + matrix, linear transfer
    if ds == ADataSpace::ADATASPACE_SCRGB_LINEAR {
        return Cicp {
            primaries: Primaries::Bt709,
            transfer: TransferFunction::Linear,
            matrix: MatrixCoefficients::Bt709,
            full_range: true,
        };
    }

    if ds == ADataSpace::ADATASPACE_DISPLAY_P3 {
        return Cicp {
            primaries: Primaries::Smpte432,
            transfer: TransferFunction::Srgb,
            matrix: MatrixCoefficients::Smpte170m,
            full_range: true,
        };
    }

    if ds == ADataSpace::ADATASPACE_DCI_P3 {
        return Cicp {
            primaries: Primaries::Smpte431,
            transfer: TransferFunction::Smpte428,
            matrix: MatrixCoefficients::Bt709,
            full_range: true,
        };
    }

    // BT.2020 SDR
    if ds == ADataSpace::ADATASPACE_BT2020 {
        return Cicp {
            primaries: Primaries::Bt2020,
            transfer: TransferFunction::Bt202010bit,
            matrix: MatrixCoefficients::Bt2020Ncl,
            full_range: true,
        };
    }

    // BT.2020 PQ — full-range variant
    if ds == ADataSpace::ADATASPACE_BT2020_PQ {
        return Cicp {
            primaries: Primaries::Bt2020,
            transfer: TransferFunction::Smpte2084,
            matrix: MatrixCoefficients::Bt2020Ncl,
            full_range: true,
        };
    }

    // BT.2020 PQ — ITU limited-range variant
    if ds == ADataSpace::ADATASPACE_BT2020_ITU_PQ {
        return Cicp {
            primaries: Primaries::Bt2020,
            transfer: TransferFunction::Smpte2084,
            matrix: MatrixCoefficients::Bt2020Ncl,
            full_range: false,
        };
    }

    // BT.2020 HLG — full-range variant
    if ds == ADataSpace::ADATASPACE_BT2020_HLG {
        return Cicp {
            primaries: Primaries::Bt2020,
            transfer: TransferFunction::Hlg,
            matrix: MatrixCoefficients::Bt2020Ncl,
            full_range: true,
        };
    }

    // BT.2020 HLG — ITU limited-range variant
    if ds == ADataSpace::ADATASPACE_BT2020_ITU_HLG {
        return Cicp {
            primaries: Primaries::Bt2020,
            transfer: TransferFunction::Hlg,
            matrix: MatrixCoefficients::Bt2020Ncl,
            full_range: false,
        };
    }

    Cicp::unspecified()
}

fn encode_heic_inner_u8(
    bitmap_data: &BitmapData,
    cicp: Cicp,
    quality: u32,
    lossless: bool,
    exif: Option<&[u8]>,
    has_real_alpha: bool,
    chroma_format: ChromaFormat,
) -> Result<Vec<u8>, anyhow::Error> {
    dbg_log!(
        debug,
        "encode_heic_inner_u8: {}x{} pixels={} quality={} lossless={} exif={}",
        bitmap_data.width,
        bitmap_data.height,
        bitmap_data.data.len(),
        quality,
        lossless,
        exif.map_or_else(|| "none".to_string(), |e| format!("{} bytes", e.len()))
    );

    let mut local_cicp = cicp;
    dbg_log!(
        debug,
        "cicp in: primaries={:?} transfer={:?} matrix={:?} full_range={}",
        cicp.primaries,
        cicp.transfer,
        cicp.matrix,
        cicp.full_range
    );
    dbg_log!(debug, "has_real_alpha={has_real_alpha}");

    let yuv_range = match cicp.full_range {
        true => YuvRange::Full,
        false => YuvRange::Limited,
    };
    let yuv_matrix = match cicp.matrix {
        MatrixCoefficients::Bt709 => YuvStandardMatrix::Bt709,
        MatrixCoefficients::Unspecified => YuvStandardMatrix::Bt601,
        MatrixCoefficients::Fcc => YuvStandardMatrix::Fcc,
        MatrixCoefficients::Smpte170m => YuvStandardMatrix::Bt601,
        MatrixCoefficients::Bt2020Ncl => YuvStandardMatrix::Bt2020,
        _other => {
            dbg_log!(
                warn,
                "unhandled MatrixCoefficients::{:?} — falling back to Bt601",
                _other
            );
            YuvStandardMatrix::Bt601
        }
    };
    dbg_log!(debug, "yuv_range={yuv_range:?} yuv_matrix={yuv_matrix:?}");

    let mut planar_image = YuvPlanarImageMut::alloc(
        bitmap_data.width as u32,
        bitmap_data.height as u32,
        match chroma_format {
            ChromaFormat::Monochrome => unreachable!("Should be sorted out in different place"),
            ChromaFormat::Yuv420 => YuvChromaSubsampling::Yuv420,
            ChromaFormat::Yuv422 => YuvChromaSubsampling::Yuv422,
            ChromaFormat::Yuv444 => YuvChromaSubsampling::Yuv444,
        },
    );
    dbg_log!(
        debug,
        "allocated YUV420 planes: {}x{} (y={} u={} v={} samples)",
        planar_image.width,
        planar_image.height,
        planar_image.y_plane.borrow().len(),
        planar_image.u_plane.borrow().len(),
        planar_image.v_plane.borrow().len(),
    );

    if !lossless {
        dbg_log!(debug, "RGB→YUV path: yuv420 balanced (lossless)");
        let f = match chroma_format {
            ChromaFormat::Monochrome => unreachable!(),
            ChromaFormat::Yuv420 => rgba_to_yuv420,
            ChromaFormat::Yuv422 => rgba_to_yuv422,
            ChromaFormat::Yuv444 => rgba_to_yuv444,
        };
        f(
            &mut planar_image,
            &bitmap_data.data,
            bitmap_data.width as u32 * 4,
            yuv_range,
            yuv_matrix,
            YuvConversionMode::Balanced,
        )
        .map_err(|x| {
            dbg_log!(error, "rgba_to_yuv420 failed: {x}");
            anyhow::anyhow!(x)
        })?;
    } else {
        dbg_log!(
            debug,
            "RGB→YUV path: ycgco420 (lossy) — overriding matrix to YCgCo"
        );
        local_cicp.matrix = MatrixCoefficients::YCgCo;
        let f = match chroma_format {
            ChromaFormat::Monochrome => unreachable!(),
            ChromaFormat::Yuv420 => rgba_to_ycgco420,
            ChromaFormat::Yuv422 => rgba_to_ycgco422,
            ChromaFormat::Yuv444 => rgba_to_ycgco444,
        };
        f(
            &mut planar_image,
            &bitmap_data.data,
            bitmap_data.width as u32 * 4,
            yuv_range,
        )
        .map_err(|x| {
            dbg_log!(error, "rgba_to_ycgco420 failed: {x}");
            anyhow::anyhow!(x)
        })?;
    }

    dbg_log!(
        debug,
        "cicp after conversion: primaries={:?} transfer={:?} matrix={:?} full_range={}",
        local_cicp.primaries,
        local_cicp.transfer,
        local_cicp.matrix,
        local_cicp.full_range
    );

    let yuv = hpvca::Yuv {
        y: planar_image
            .y_plane
            .borrow()
            .iter()
            .map(|&x| x as u16)
            .collect(),
        cb: planar_image
            .u_plane
            .borrow()
            .iter()
            .map(|&x| x as u16)
            .collect(),
        cr: planar_image
            .v_plane
            .borrow()
            .iter()
            .map(|&x| x as u16)
            .collect(),
        width: planar_image.width,
        height: planar_image.height,
        display_w: planar_image.width,
        display_h: planar_image.height,
        chroma: chroma_format,
        bit_depth: BitDepth::Eight,
    };
    dbg_log!(
        debug,
        "yuv planes packed: y={} cb={} cr={} samples",
        yuv.y.len(),
        yuv.cb.len(),
        yuv.cr.len()
    );

    let threads = available_parallelism()
        .unwrap_or(NonZero::new(1).unwrap())
        .get();
    dbg_log!(debug, "encoder threads={threads}");

    let mut config = hpvca::EncodeConfig::new()
        .with_cicp(local_cicp)
        .with_quality(quality as u8)
        .with_threads(threads)
        .with_lossless(lossless);

    if let Some(exif) = exif {
        dbg_log!(debug, "attaching exif: {} bytes", exif.len());
        config = config.with_exif(exif.to_vec());
    }

    if has_real_alpha {
        dbg_log!(debug, "encoding with alpha plane");
        let alpha = bitmap_data
            .data
            .as_chunks::<4>()
            .0
            .iter()
            .map(|x| x[3] as u16)
            .collect::<Vec<_>>();

        dbg_log!(debug, "alpha plane: {} samples", alpha.len());
        let result = hpvca::encode_yuv_with_alpha(&yuv, &alpha, &config).map_err(|x| {
            dbg_log!(error, "encode_yuv_with_alpha failed: {x}");
            anyhow::anyhow!(x)
        })?;
        dbg_log!(debug, "encoded with alpha: {} bytes", result.len());
        return Ok(result);
    }

    let result = hpvca::encode_yuv(&yuv, &config).map_err(|x| {
        dbg_log!(error, "encode_yuv failed: {x}");
        anyhow::anyhow!(x)
    })?;
    dbg_log!(
        debug,
        "encoded: {} bytes ({:.2} bpp)",
        result.len(),
        (result.len() * 8) as f64 / (bitmap_data.width * bitmap_data.height) as f64
    );
    Ok(result)
}

fn encode_heic_inner_u16_10_bit(
    hd_plane: &[u16],
    bitmap_data: &BitmapData,
    cicp: Cicp,
    quality: u32,
    lossless: bool,
    exif: Option<&[u8]>,
    has_real_alpha: bool,
    chroma_format: ChromaFormat,
) -> Result<Vec<u8>, anyhow::Error> {
    dbg_log!(
        debug,
        "encode_heic_inner_u16_10_bit: {}x{} pixels={} quality={} lossless={} exif={}",
        bitmap_data.width,
        bitmap_data.height,
        hd_plane.len(),
        quality,
        lossless,
        exif.map_or_else(|| "none".to_string(), |e| format!("{} bytes", e.len()))
    );

    let mut local_cicp = cicp;
    dbg_log!(
        debug,
        "cicp in: primaries={:?} transfer={:?} matrix={:?} full_range={}",
        cicp.primaries,
        cicp.transfer,
        cicp.matrix,
        cicp.full_range
    );
    dbg_log!(debug, "has_real_alpha={has_real_alpha}");

    let yuv_range = match cicp.full_range {
        true => YuvRange::Full,
        false => YuvRange::Limited,
    };
    let yuv_matrix = match cicp.matrix {
        MatrixCoefficients::Bt709 => YuvStandardMatrix::Bt709,
        MatrixCoefficients::Unspecified => YuvStandardMatrix::Bt601,
        MatrixCoefficients::Fcc => YuvStandardMatrix::Fcc,
        MatrixCoefficients::Smpte170m => YuvStandardMatrix::Bt601,
        MatrixCoefficients::Bt2020Ncl => YuvStandardMatrix::Bt2020,
        _other => {
            dbg_log!(
                warn,
                "unhandled MatrixCoefficients::{:?} — falling back to Bt601",
                _other
            );
            YuvStandardMatrix::Bt601
        }
    };
    dbg_log!(debug, "yuv_range={yuv_range:?} yuv_matrix={yuv_matrix:?}");

    let mut planar_image = YuvPlanarImageMut::alloc(
        bitmap_data.width as u32,
        bitmap_data.height as u32,
        match chroma_format {
            ChromaFormat::Monochrome => unreachable!("Should be sorted out in different place"),
            ChromaFormat::Yuv420 => YuvChromaSubsampling::Yuv420,
            ChromaFormat::Yuv422 => YuvChromaSubsampling::Yuv422,
            ChromaFormat::Yuv444 => YuvChromaSubsampling::Yuv444,
        },
    );
    dbg_log!(
        debug,
        "allocated YUV420 planes: {}x{} (y={} u={} v={} samples)",
        planar_image.width,
        planar_image.height,
        planar_image.y_plane.borrow().len(),
        planar_image.u_plane.borrow().len(),
        planar_image.v_plane.borrow().len(),
    );

    if !lossless {
        dbg_log!(debug, "RGB→YUV path: yuv420 (lossy)");
        let f = match chroma_format {
            ChromaFormat::Monochrome => unreachable!(),
            ChromaFormat::Yuv420 => rgba10_to_i010,
            ChromaFormat::Yuv422 => rgba10_to_i210,
            ChromaFormat::Yuv444 => rgba10_to_i410,
        };
        f(
            &mut planar_image,
            hd_plane,
            bitmap_data.width as u32 * 4,
            yuv_range,
            yuv_matrix,
        )
        .map_err(|x| {
            dbg_log!(error, "rgba10_to_i010 failed: {x}");
            anyhow::anyhow!(x)
        })?;
    } else {
        dbg_log!(
            debug,
            "RGB→YUV path: ycgco420 (lossy) — overriding matrix to YCgCo"
        );
        local_cicp.matrix = MatrixCoefficients::YCgCo;
        let f = match chroma_format {
            ChromaFormat::Monochrome => unreachable!(),
            ChromaFormat::Yuv420 => rgba10_to_icgc010,
            ChromaFormat::Yuv422 => rgba10_to_icgc210,
            ChromaFormat::Yuv444 => rgba10_to_icgc410,
        };
        f(
            &mut planar_image,
            hd_plane,
            bitmap_data.width as u32 * 4,
            yuv_range,
        )
        .map_err(|x| {
            dbg_log!(error, "rgba10_to_icgc010 failed: {x}");
            anyhow::anyhow!(x)
        })?;
    }

    dbg_log!(
        debug,
        "cicp after conversion: primaries={:?} transfer={:?} matrix={:?} full_range={}",
        local_cicp.primaries,
        local_cicp.transfer,
        local_cicp.matrix,
        local_cicp.full_range
    );

    let yuv = hpvca::Yuv {
        y: planar_image.y_plane.borrow().to_vec(),
        cb: planar_image.u_plane.borrow().to_vec(),
        cr: planar_image.v_plane.borrow().to_vec(),
        width: planar_image.width,
        height: planar_image.height,
        display_w: planar_image.width,
        display_h: planar_image.height,
        chroma: chroma_format,
        bit_depth: BitDepth::Ten,
    };
    dbg_log!(
        debug,
        "yuv planes packed: y={} cb={} cr={} samples",
        yuv.y.len(),
        yuv.cb.len(),
        yuv.cr.len()
    );

    let threads = available_parallelism()
        .unwrap_or(NonZero::new(1).unwrap())
        .get();
    dbg_log!(debug, "encoder threads={threads}");

    let mut config = hpvca::EncodeConfig::new()
        .with_cicp(local_cicp)
        .with_quality(quality as u8)
        .with_threads(threads)
        .with_lossless(lossless);

    if let Some(exif) = exif {
        dbg_log!(debug, "attaching exif: {} bytes", exif.len());
        config = config.with_exif(exif.to_vec());
    }

    if has_real_alpha {
        dbg_log!(debug, "encoding with alpha plane");
        let alpha = bitmap_data
            .data
            .as_chunks::<4>()
            .0
            .iter()
            .map(|x| x[3] as u16)
            .collect::<Vec<_>>();

        dbg_log!(debug, "alpha plane: {} samples", alpha.len());
        let result = hpvca::encode_yuv_with_alpha(&yuv, &alpha, &config).map_err(|x| {
            dbg_log!(error, "encode_yuv_with_alpha failed: {x}");
            anyhow::anyhow!(x)
        })?;
        dbg_log!(debug, "encoded with alpha: {} bytes", result.len());
        return Ok(result);
    }

    let result = hpvca::encode_yuv(&yuv, &config).map_err(|x| {
        dbg_log!(error, "encode_yuv failed: {x}");
        anyhow::anyhow!(x)
    })?;
    dbg_log!(
        debug,
        "encoded: {} bytes ({:.2} bpp)",
        result.len(),
        (result.len() * 8) as f64 / (bitmap_data.width * bitmap_data.height) as f64
    );
    Ok(result)
}

fn encode_heic_inner(
    bitmap_data: &mut BitmapData,
    cicp: Cicp,
    quality: u32,
    lossless: bool,
    exif: Option<&[u8]>,
    chroma_format: ChromaFormat,
) -> Result<Vec<u8>, anyhow::Error> {
    dbg_log!(debug, "encode_heic_inner: format={:?}", bitmap_data.format);
    match bitmap_data.format {
        BitmapPixelFormat::Rgba8888 => {
            let has_real_alpha =
                has_non_constant_alpha::<u8, u32, 3, 4>(&bitmap_data.data, bitmap_data.width);
            encode_heic_inner_u8(
                bitmap_data,
                cicp,
                quality,
                lossless,
                exif,
                has_real_alpha,
                chroma_format,
            )
        }
        BitmapPixelFormat::Rgb565 => {
            let rgba8888 = rgb565_bytes_to_rgba8888(&bitmap_data.data);
            bitmap_data.data = rgba8888;
            encode_heic_inner_u8(
                bitmap_data,
                cicp,
                quality,
                lossless,
                exif,
                false,
                chroma_format,
            )
        }
        BitmapPixelFormat::RgbaF16 => {
            let hd_plane =
                f16_bytes_to_rgba10(&bitmap_data.data, bitmap_data.width, bitmap_data.height)?;
            encode_heic_inner_u16_10_bit(
                &hd_plane,
                bitmap_data,
                cicp,
                quality,
                lossless,
                exif,
                false,
                chroma_format,
            )
        }
        BitmapPixelFormat::Rgba1010102 => {
            let hd_plane = ar30_bytes_to_rgba10(&bitmap_data.data);
            encode_heic_inner_u16_10_bit(
                &hd_plane,
                bitmap_data,
                cicp,
                quality,
                lossless,
                exif,
                false,
                chroma_format,
            )
        }
        BitmapPixelFormat::A8 => {
            dbg_log!(error, "A8 format is not supported for encoding");
            Err(anyhow::anyhow!(WeaverError::PixelFormatIsNotSupported(
                "BitmapPixelFormat::A8".to_string()
            )))
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn encode_heic_file(
    env: *mut jni::sys::JNIEnv,
    image: jobject,
    exif: jobject,
    color_space: i32,
    quality: i32,
    chroma_subsampling_code: i32,
    lossless: bool,
) -> jbyteArray {
    init_logging();

    let chroma_subsampling = match chroma_subsampling_code {
        2 => ChromaFormat::Yuv422,
        3 => ChromaFormat::Yuv444,
        4 => ChromaFormat::Monochrome,
        _ => ChromaFormat::Yuv420,
    };

    dbg_log!(
        debug,
        "encode_heic_file: color_space={color_space} quality={quality} lossless={lossless} \
        chroma={:?} image_null={} exif_null={}",
        chroma_subsampling,
        image.is_null(),
        exif.is_null()
    );

    let mut unowned = unsafe { EnvUnowned::from_raw(env) };

    let outcome = unowned.with_env(|env| -> Result<jobject, anyhow::Error> {
        if chroma_subsampling == ChromaFormat::Monochrome {
            dbg_log!(
                error,
                "encode_heic_file failed: monochrome in heic currently is not supported"
            );
            let _ = env.throw_new(
                jni_str!("java/lang/RuntimeException"),
                JNIString::from(WeaverError::MonochromeIsNotSupported.to_string()),
            );
            return Ok(JObject::null().into_raw());
        }
        let mut encoding_result = || -> Result<jobject, anyhow::Error> {
            let quality = quality.clamp(1, 100) as u32;
            dbg_log!(debug, "clamped quality={quality}");

            let cicp = resolve_cicp(color_space);
            dbg_log!(
                debug,
                "resolved cicp: primaries={:?} transfer={:?} matrix={:?} full_range={}",
                cicp.primaries,
                cicp.transfer,
                cicp.matrix,
                cicp.full_range
            );

            let mut bitmap_data = unsafe {
                get_bitmap_data(env, image).map_err(|x| {
                    dbg_log!(error, "get_bitmap_data failed: {x}");
                    anyhow::anyhow!(x)
                })?
            };
            dbg_log!(
                debug,
                "bitmap: {}x{} format={:?} data={} bytes",
                bitmap_data.width,
                bitmap_data.height,
                bitmap_data.format,
                bitmap_data.data.len()
            );

            let exif_data = optional_bytebuffer_to_vec(env, exif).map_err(|x| {
                dbg_log!(error, "optional_bytebuffer_to_vec failed: {x}");
                anyhow::anyhow!(x)
            })?;
            dbg_log!(
                debug,
                "exif: {}",
                exif_data
                    .as_ref()
                    .map_or_else(|| "none".to_string(), |e| format!("{} bytes", e.len()))
            );

            let encoded_data = encode_heic_inner(
                &mut bitmap_data,
                cicp,
                quality,
                lossless,
                exif_data.as_deref(),
                chroma_subsampling,
            )?;
            dbg_log!(
                debug,
                "encode complete: {} bytes output",
                encoded_data.len()
            );

            let arr = env.byte_array_from_slice(&encoded_data).map_err(|e| {
                dbg_log!(error, "byte_array_from_slice failed: {e}");
                anyhow::anyhow!(e)
            })?;
            dbg_log!(debug, "java byte[] created successfully");
            Ok(arr.into_raw())
        };
        let result = encoding_result();
        match result {
            Ok(obj) => {
                dbg_log!(debug, "encode_heic_file: success");
                Ok(obj)
            }
            Err(e) => {
                dbg_log!(error, "encode_heic_file failed: {e:#}");
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
        Outcome::Ok(v) => {
            dbg_log!(debug, "encode_heic_file: success");
            v
        }
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
