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
use crate::av1_encode_android::{AvEncodingConfig, AvEncodingSpeed, resolve_cicp_maroontree};
use crate::cvt::{ar30_bytes_to_rgba10, f16_bytes_to_rgba10, rgb565_bytes_to_rgba8888};
use crate::ffi::{BitmapData, BitmapPixelFormat, get_bitmap_data};
use crate::heic_decode::WeaverError;
use crate::support::{dbg_log, has_non_constant_alpha, init_logging, optional_bytebuffer_to_vec};
use jni::objects::JObject;
use jni::strings::JNIString;
use jni::sys::{jbyteArray, jobject};
use jni::{EnvUnowned, Outcome, jni_str};
use maroontree::{BitDepth, ChromaFormat, MatrixCoefficients, PlanarImage};
use std::num::NonZero;
use std::ptr::null_mut;
use std::thread::available_parallelism;
use yuv::{
    YuvChromaSubsampling, YuvConversionMode, YuvGrayImageMut, YuvPlanarImageMut, YuvRange,
    YuvStandardMatrix, rgba_to_ycgco420, rgba_to_ycgco422, rgba_to_ycgco444, rgba_to_yuv400,
    rgba_to_yuv420, rgba_to_yuv422, rgba_to_yuv444, rgba10_to_i010, rgba10_to_i210, rgba10_to_i410,
    rgba10_to_icgc010, rgba10_to_icgc210, rgba10_to_icgc410, rgba10_to_y010,
};

fn encode_av2_inner_mono_u8(
    bitmap_data: &BitmapData,
    config: &AvEncodingConfig,
    has_real_alpha: bool,
) -> Result<Vec<u8>, anyhow::Error> {
    let cicp = config.cicp;
    let quality = config.quality;
    let _lossless = config.lossless;
    let exif = config.exif.as_ref();
    dbg_log!(
        debug,
        "encode_av2_inner_mono_u8: {}x{} pixels={} quality={} lossless={} exif={}",
        bitmap_data.width,
        bitmap_data.height,
        bitmap_data.data.len(),
        quality,
        _lossless,
        exif.map_or_else(|| "none".to_string(), |e| format!("{} bytes", e.len()))
    );

    let local_cicp = cicp;
    dbg_log!(
        debug,
        "cicp: primaries={:?} transfer={:?} matrix={:?} full_range={}",
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
                "encode_av2_inner_mono_u8: unhandled matrix={:?} — falling back to Bt601",
                _other
            );
            YuvStandardMatrix::Bt601
        }
    };
    dbg_log!(debug, "yuv_range={yuv_range:?} yuv_matrix={yuv_matrix:?}");

    let mut planar_image =
        YuvGrayImageMut::alloc(bitmap_data.width as u32, bitmap_data.height as u32);
    dbg_log!(
        debug,
        "allocated monochrome plane: {}x{} y={} samples",
        planar_image.width,
        planar_image.height,
        planar_image.y_plane.borrow().len()
    );

    rgba_to_yuv400(
        &mut planar_image,
        &bitmap_data.data,
        bitmap_data.width as u32 * 4,
        yuv_range,
        yuv_matrix,
    )
    .map_err(|x| {
        dbg_log!(error, "rgba_to_yuv400 failed: {x}");
        anyhow::anyhow!(x)
    })?;

    let threads = available_parallelism()
        .unwrap_or(NonZero::new(1).unwrap())
        .get();
    dbg_log!(debug, "encoder threads={threads}");

    let mut config = maroontree::EncodeConfig::new()
        .with_cicp(local_cicp)
        .with_quality(quality as u8)
        .with_threads(threads)
        .with_chroma(ChromaFormat::Monochrome)
        .with_speed(config.speed.to_maroontree());

    if let Some(exif) = exif {
        dbg_log!(debug, "attaching exif: {} bytes", exif.len());
        config = config.with_exif(exif.to_vec());
    }

    if has_real_alpha {
        dbg_log!(debug, "monochrome+alpha: extracting alpha plane");
        let alpha = bitmap_data
            .data
            .as_chunks::<4>()
            .0
            .iter()
            .map(|x| x[3])
            .collect::<Vec<_>>();
        dbg_log!(debug, "alpha plane: {} samples", alpha.len());

        let av2_planar_image = PlanarImage {
            width: bitmap_data.width,
            height: bitmap_data.height,
            planes: [
                planar_image.y_plane.borrow().to_vec(),
                alpha,
                vec![],
                vec![],
            ],
            bit_depth: BitDepth::Eight,
        };
        let result = maroontree::av2_image::encode_gray_alpha8(&av2_planar_image, &config)
            .map_err(|x| {
                dbg_log!(error, "encode_gray_alpha8 failed: {x}");
                anyhow::anyhow!(x)
            })?;
        dbg_log!(debug, "encoded monochrome+alpha: {} bytes", result.len());
        return Ok(result);
    }

    let av2_planar_image = PlanarImage {
        width: bitmap_data.width,
        height: bitmap_data.height,
        planes: [
            planar_image.y_plane.borrow().to_vec(),
            vec![],
            vec![],
            vec![],
        ],
        bit_depth: BitDepth::Eight,
    };
    let result = maroontree::av2_image::encode_gray8(&av2_planar_image, &config).map_err(|x| {
        dbg_log!(error, "encode_gray8 failed: {x}");
        anyhow::anyhow!(x)
    })?;
    dbg_log!(
        debug,
        "encoded monochrome: {} bytes ({:.2} bpp)",
        result.len(),
        (result.len() * 8) as f64 / (bitmap_data.width * bitmap_data.height) as f64
    );
    Ok(result)
}

fn encode_av2_inner_mono_u16(
    hd_data: &[u16],
    bitmap_data: &BitmapData,
    config: &AvEncodingConfig,
    has_real_alpha: bool,
) -> Result<Vec<u8>, anyhow::Error> {
    let cicp = config.cicp;
    let quality = config.quality;
    let _lossless = config.lossless;
    let exif = config.exif.as_ref();
    dbg_log!(
        debug,
        "encode_av2_inner_mono_u8: {}x{} pixels={} quality={} lossless={} exif={}",
        bitmap_data.width,
        bitmap_data.height,
        bitmap_data.data.len(),
        quality,
        _lossless,
        exif.map_or_else(|| "none".to_string(), |e| format!("{} bytes", e.len()))
    );

    let local_cicp = cicp;
    dbg_log!(
        debug,
        "cicp: primaries={:?} transfer={:?} matrix={:?} full_range={}",
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
                "encode_av2_inner_mono_u8: unhandled matrix={:?} — falling back to Bt601",
                _other
            );
            YuvStandardMatrix::Bt601
        }
    };
    dbg_log!(debug, "yuv_range={yuv_range:?} yuv_matrix={yuv_matrix:?}");

    let mut planar_image =
        YuvGrayImageMut::alloc(bitmap_data.width as u32, bitmap_data.height as u32);
    dbg_log!(
        debug,
        "allocated monochrome plane: {}x{} y={} samples",
        planar_image.width,
        planar_image.height,
        planar_image.y_plane.borrow().len()
    );

    rgba10_to_y010(
        &mut planar_image,
        hd_data,
        bitmap_data.width as u32 * 4,
        yuv_range,
        yuv_matrix,
    )
    .map_err(|x| {
        dbg_log!(error, "rgba_to_yuv400 failed: {x}");
        anyhow::anyhow!(x)
    })?;

    let threads = available_parallelism()
        .unwrap_or(NonZero::new(1).unwrap())
        .get();
    dbg_log!(debug, "encoder threads={threads}");

    let mut config = maroontree::EncodeConfig::new()
        .with_cicp(local_cicp)
        .with_quality(quality as u8)
        .with_threads(threads)
        .with_chroma(ChromaFormat::Monochrome)
        .with_speed(config.speed.to_maroontree());

    if let Some(exif) = exif {
        dbg_log!(debug, "attaching exif: {} bytes", exif.len());
        config = config.with_exif(exif.to_vec());
    }

    if has_real_alpha {
        dbg_log!(debug, "monochrome+alpha: extracting alpha plane");
        let alpha = hd_data
            .as_chunks::<4>()
            .0
            .iter()
            .map(|x| x[3])
            .collect::<Vec<_>>();
        dbg_log!(debug, "alpha plane: {} samples", alpha.len());

        let av2_planar_image = PlanarImage {
            width: bitmap_data.width,
            height: bitmap_data.height,
            planes: [
                planar_image.y_plane.borrow().to_vec(),
                alpha,
                vec![],
                vec![],
            ],
            bit_depth: BitDepth::Ten,
        };
        let result = maroontree::av2_image::encode_gray_alpha10(&av2_planar_image, &config)
            .map_err(|x| {
                dbg_log!(error, "encode_gray_alpha8 failed: {x}");
                anyhow::anyhow!(x)
            })?;
        dbg_log!(debug, "encoded monochrome+alpha: {} bytes", result.len());
        return Ok(result);
    }

    let av2_planar_image = PlanarImage {
        width: bitmap_data.width,
        height: bitmap_data.height,
        planes: [
            planar_image.y_plane.borrow().to_vec(),
            vec![],
            vec![],
            vec![],
        ],
        bit_depth: BitDepth::Ten,
    };
    let result = maroontree::av2_image::encode_gray10(&av2_planar_image, &config).map_err(|x| {
        dbg_log!(error, "encode_gray8 failed: {x}");
        anyhow::anyhow!(x)
    })?;
    dbg_log!(
        debug,
        "encoded monochrome: {} bytes ({:.2} bpp)",
        result.len(),
        (result.len() * 8) as f64 / (bitmap_data.width * bitmap_data.height) as f64
    );
    Ok(result)
}

fn encode_av2_inner_u8(
    bitmap_data: &BitmapData,
    config: &AvEncodingConfig,
    has_real_alpha: bool,
) -> Result<Vec<u8>, anyhow::Error> {
    let cicp = config.cicp;
    let quality = config.quality;
    let lossless = config.lossless;
    let chroma_subsampling = config.chroma;
    let exif = config.exif.as_ref();
    dbg_log!(
        debug,
        "encode_av2_inner_u8: {}x{} pixels={} quality={} lossless={} \
         chroma={:?} exif={}",
        bitmap_data.width,
        bitmap_data.height,
        bitmap_data.data.len(),
        quality,
        lossless,
        chroma_subsampling,
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

    if chroma_subsampling == ChromaFormat::Monochrome {
        dbg_log!(debug, "delegating to monochrome path");
        return encode_av2_inner_mono_u8(bitmap_data, config, has_real_alpha);
    }

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
                "encode_av2_inner_u8: unhandled matrix={:?} — falling back to Bt601",
                _other
            );
            YuvStandardMatrix::Bt601
        }
    };
    dbg_log!(debug, "yuv_range={yuv_range:?} yuv_matrix={yuv_matrix:?}");

    let mut planar_image = YuvPlanarImageMut::alloc(
        bitmap_data.width as u32,
        bitmap_data.height as u32,
        match chroma_subsampling {
            ChromaFormat::Yuv420 => YuvChromaSubsampling::Yuv420,
            ChromaFormat::Yuv422 => YuvChromaSubsampling::Yuv422,
            ChromaFormat::Yuv444 => YuvChromaSubsampling::Yuv444,
            ChromaFormat::Monochrome => unreachable!(),
        },
    );
    dbg_log!(
        debug,
        "allocated {:?} planes: {}x{} (y={} u={} v={} samples)",
        chroma_subsampling,
        planar_image.width,
        planar_image.height,
        planar_image.y_plane.borrow().len(),
        planar_image.u_plane.borrow().len(),
        planar_image.v_plane.borrow().len(),
    );

    if !lossless {
        dbg_log!(
            debug,
            "RGB→YUV path: standard YUV balanced (lossy), subsampling={:?}",
            chroma_subsampling
        );
        let f = match chroma_subsampling {
            ChromaFormat::Yuv420 => rgba_to_yuv420,
            ChromaFormat::Yuv422 => rgba_to_yuv422,
            ChromaFormat::Yuv444 => rgba_to_yuv444,
            ChromaFormat::Monochrome => unreachable!(),
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
            dbg_log!(error, "rgba_to_yuv{:?} failed: {x}", chroma_subsampling);
            anyhow::anyhow!(x)
        })?;
    } else {
        dbg_log!(
            debug,
            "RGB→YUV path: YCgCo (lossless), subsampling={:?} — overriding matrix to YCgCo",
            chroma_subsampling
        );
        local_cicp.matrix = MatrixCoefficients::YCgCo;
        let f = match chroma_subsampling {
            ChromaFormat::Yuv420 => rgba_to_ycgco420,
            ChromaFormat::Yuv422 => rgba_to_ycgco422,
            ChromaFormat::Yuv444 => rgba_to_ycgco444,
            ChromaFormat::Monochrome => unreachable!(),
        };
        f(
            &mut planar_image,
            &bitmap_data.data,
            bitmap_data.width as u32 * 4,
            yuv_range,
        )
        .map_err(|x| {
            dbg_log!(error, "rgba_to_ycgco{:?} failed: {x}", chroma_subsampling);
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
    dbg_log!(
        debug,
        "yuv planes: y={} cb={} cr={} samples",
        planar_image.y_plane.borrow().len(),
        planar_image.u_plane.borrow().len(),
        planar_image.v_plane.borrow().len()
    );

    let threads = available_parallelism()
        .unwrap_or(NonZero::new(1).unwrap())
        .get();
    dbg_log!(debug, "encoder threads={threads}");

    let mut config = maroontree::EncodeConfig::new()
        .with_cicp(local_cicp)
        .with_quality(quality as u8)
        .with_threads(threads)
        .with_chroma(chroma_subsampling)
        .with_speed(config.speed.to_maroontree());

    if let Some(exif) = exif {
        dbg_log!(debug, "attaching exif: {} bytes", exif.len());
        config = config.with_exif(exif.to_vec());
    }

    if has_real_alpha {
        dbg_log!(debug, "encoding {:?} with alpha plane", chroma_subsampling);
        let alpha = bitmap_data
            .data
            .as_chunks::<4>()
            .0
            .iter()
            .map(|x| x[3])
            .collect::<Vec<_>>();
        dbg_log!(debug, "alpha plane: {} samples", alpha.len());

        let av2_planar_image = PlanarImage {
            width: bitmap_data.width,
            height: bitmap_data.height,
            planes: [
                planar_image.y_plane.borrow().to_vec(),
                planar_image.u_plane.borrow().to_vec(),
                planar_image.v_plane.borrow().to_vec(),
                alpha,
            ],
            bit_depth: BitDepth::Eight,
        };
        let result = maroontree::av2_image::encode_yuva8_with_alpha(&av2_planar_image, &config)
            .map_err(|x| {
                dbg_log!(error, "encode_yuva8_with_alpha failed: {x}");
                anyhow::anyhow!(x)
            })?;
        dbg_log!(
            debug,
            "encoded {:?}+alpha: {} bytes",
            chroma_subsampling,
            result.len()
        );
        return Ok(result);
    }

    let av2_planar_image = PlanarImage {
        width: bitmap_data.width,
        height: bitmap_data.height,
        planes: [
            planar_image.y_plane.borrow().to_vec(),
            planar_image.u_plane.borrow().to_vec(),
            planar_image.v_plane.borrow().to_vec(),
            vec![],
        ],
        bit_depth: BitDepth::Eight,
    };
    let result = maroontree::av2_image::encode_yuv8(&av2_planar_image, &config).map_err(|x| {
        dbg_log!(error, "encode_yuv8 failed: {x}");
        anyhow::anyhow!(x)
    })?;
    dbg_log!(
        debug,
        "encoded {:?}: {} bytes ({:.2} bpp)",
        chroma_subsampling,
        result.len(),
        (result.len() * 8) as f64 / (bitmap_data.width * bitmap_data.height) as f64
    );
    Ok(result)
}

fn encode_av2_inner_u16_10_bit(
    hd_plane: &[u16],
    bitmap_data: &BitmapData,
    config: &AvEncodingConfig,
    has_real_alpha: bool,
) -> Result<Vec<u8>, anyhow::Error> {
    let cicp = config.cicp;
    let quality = config.quality;
    let lossless = config.lossless;
    let chroma_format = config.chroma;
    let exif = config.exif.as_ref();
    if chroma_format == ChromaFormat::Monochrome {
        return encode_av2_inner_mono_u16(hd_plane, bitmap_data, config, has_real_alpha);
    }
    dbg_log!(
        debug,
        "encode_heic_inner_u16_10_bit: {}x{} hd_pixels={} quality={} lossless={} \
         chroma={:?} exif={}",
        bitmap_data.width,
        bitmap_data.height,
        hd_plane.len(),
        quality,
        lossless,
        chroma_format,
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
                "encode_heic_inner_u16_10_bit: unhandled matrix={:?} — falling back to Bt601",
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
            ChromaFormat::Yuv420 => YuvChromaSubsampling::Yuv420,
            ChromaFormat::Yuv422 => YuvChromaSubsampling::Yuv422,
            ChromaFormat::Yuv444 => YuvChromaSubsampling::Yuv444,
            ChromaFormat::Monochrome => unreachable!(),
        },
    );
    dbg_log!(
        debug,
        "allocated 10-bit {:?} planes: {}x{} (y={} u={} v={} samples)",
        chroma_format,
        planar_image.width,
        planar_image.height,
        planar_image.y_plane.borrow().len(),
        planar_image.u_plane.borrow().len(),
        planar_image.v_plane.borrow().len(),
    );

    if !lossless {
        dbg_log!(
            debug,
            "RGB10→YUV path: i010/i210/i410 balanced (lossy), chroma={:?}",
            chroma_format
        );
        let f = match chroma_format {
            ChromaFormat::Yuv420 => rgba10_to_i010,
            ChromaFormat::Yuv422 => rgba10_to_i210,
            ChromaFormat::Yuv444 => rgba10_to_i410,
            ChromaFormat::Monochrome => unreachable!(),
        };
        f(
            &mut planar_image,
            hd_plane,
            bitmap_data.width as u32 * 4,
            yuv_range,
            yuv_matrix,
        )
        .map_err(|x| {
            dbg_log!(error, "rgba10_to_i{:?}_10bit failed: {x}", chroma_format);
            anyhow::anyhow!(x)
        })?;
    } else {
        dbg_log!(
            debug,
            "RGB10→YUV path: icgco010/210/410 (lossless), chroma={:?} — overriding matrix to YCgCo",
            chroma_format
        );
        local_cicp.matrix = MatrixCoefficients::YCgCo;
        let f = match chroma_format {
            ChromaFormat::Yuv420 => rgba10_to_icgc010,
            ChromaFormat::Yuv422 => rgba10_to_icgc210,
            ChromaFormat::Yuv444 => rgba10_to_icgc410,
            ChromaFormat::Monochrome => unreachable!(),
        };
        f(
            &mut planar_image,
            hd_plane,
            bitmap_data.width as u32 * 4,
            yuv_range,
        )
        .map_err(|x| {
            dbg_log!(
                error,
                "rgba10_to_icgco{:?}_10bit failed: {x}",
                chroma_format
            );
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
    dbg_log!(
        debug,
        "10-bit yuv planes: y={} cb={} cr={} samples",
        planar_image.y_plane.borrow().len(),
        planar_image.u_plane.borrow().len(),
        planar_image.v_plane.borrow().len()
    );

    let threads = available_parallelism()
        .unwrap_or(NonZero::new(1).unwrap())
        .get();
    dbg_log!(debug, "encoder threads={threads}");

    let mut config = maroontree::EncodeConfig::new()
        .with_cicp(local_cicp)
        .with_quality(quality as u8)
        .with_threads(threads)
        .with_chroma(chroma_format)
        .with_speed(config.speed.to_maroontree());

    if let Some(exif) = exif {
        dbg_log!(debug, "attaching exif: {} bytes", exif.len());
        config = config.with_exif(exif.to_vec());
    }

    if has_real_alpha {
        dbg_log!(
            debug,
            "encoding 10-bit {:?} with alpha plane",
            chroma_format
        );
        let alpha = hd_plane
            .as_chunks::<4>()
            .0
            .iter()
            .map(|x| x[3])
            .collect::<Vec<_>>();
        dbg_log!(debug, "alpha plane: {} samples", alpha.len());

        let av2_planar_image = PlanarImage {
            width: bitmap_data.width,
            height: bitmap_data.height,
            planes: [
                planar_image.y_plane.borrow().to_vec(),
                planar_image.u_plane.borrow().to_vec(),
                planar_image.v_plane.borrow().to_vec(),
                alpha,
            ],
            bit_depth: BitDepth::Ten,
        };
        let result = maroontree::av2_image::encode_yuva10_with_alpha(&av2_planar_image, &config)
            .map_err(|x| {
                dbg_log!(error, "encode_yuva10_with_alpha failed: {x}");
                anyhow::anyhow!(x)
            })?;
        dbg_log!(
            debug,
            "encoded 10-bit {:?}+alpha: {} bytes",
            chroma_format,
            result.len()
        );
        return Ok(result);
    }

    let av2_planar_image = PlanarImage {
        width: bitmap_data.width,
        height: bitmap_data.height,
        planes: [
            planar_image.y_plane.borrow().to_vec(),
            planar_image.u_plane.borrow().to_vec(),
            planar_image.v_plane.borrow().to_vec(),
            vec![],
        ],
        bit_depth: BitDepth::Ten,
    };
    let result = maroontree::av2_image::encode_yuv10(&av2_planar_image, &config).map_err(|x| {
        dbg_log!(error, "encode_yuv10 failed: {x}");
        anyhow::anyhow!(x)
    })?;
    dbg_log!(
        debug,
        "encoded 10-bit {:?}: {} bytes ({:.2} bpp)",
        chroma_format,
        result.len(),
        (result.len() * 8) as f64 / (bitmap_data.width * bitmap_data.height) as f64
    );
    Ok(result)
}

fn encode_av2_inner(
    bitmap_data: &mut BitmapData,
    config: &AvEncodingConfig,
) -> Result<Vec<u8>, anyhow::Error> {
    dbg_log!(
        debug,
        "encode_av2_inner: format={:?} chroma={:?}",
        bitmap_data.format,
        config.chroma
    );
    match bitmap_data.format {
        BitmapPixelFormat::Rgba8888 => {
            let has_real_alpha =
                has_non_constant_alpha::<u8, u32, 3, 4>(&bitmap_data.data, bitmap_data.width);
            encode_av2_inner_u8(bitmap_data, config, has_real_alpha)
        }
        BitmapPixelFormat::Rgb565 => {
            dbg_log!(
                debug,
                "encode_av2_inner: converting Rgb565 → Rgba8888 before encode"
            );
            let rgba8888 = rgb565_bytes_to_rgba8888(&bitmap_data.data);
            bitmap_data.data = rgba8888;
            encode_av2_inner_u8(bitmap_data, config, false)
        }
        BitmapPixelFormat::RgbaF16 => {
            dbg_log!(
                debug,
                "encode_av2_inner: converting RgbaF16 → RGBA10 before encode"
            );
            let hd_plane =
                f16_bytes_to_rgba10(&bitmap_data.data, bitmap_data.width, bitmap_data.height)?;
            encode_av2_inner_u16_10_bit(&hd_plane, bitmap_data, config, false)
        }
        BitmapPixelFormat::Rgba1010102 => {
            dbg_log!(
                debug,
                "encode_av2_inner: unpacking AR30 → RGBA10 before encode"
            );
            let hd_plane = ar30_bytes_to_rgba10(&bitmap_data.data);
            encode_av2_inner_u16_10_bit(&hd_plane, bitmap_data, config, false)
        }
        BitmapPixelFormat::A8 => {
            dbg_log!(error, "encode_av2_inner: A8 format is not supported");
            Err(anyhow::anyhow!(WeaverError::PixelFormatIsNotSupported(
                "BitmapPixelFormat::A8".to_string()
            )))
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn encode_avif_av2_file(
    env: *mut jni::sys::JNIEnv,
    image: jobject,
    exif: jobject,
    color_space: i32,
    quality: i32,
    lossless: bool,
    chroma_subsampling_code: i32,
    speed: AvEncodingSpeed,
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
        "encode_avif_av2_file: color_space={color_space} quality={quality} lossless={lossless} chroma={} ({chroma_subsampling_code}) \
         image_null={} exif_null={}",
        match chroma_subsampling {
            ChromaFormat::Yuv420 => "4:2:0",
            ChromaFormat::Yuv422 => "4:2:2",
            ChromaFormat::Yuv444 => "4:4:4",
            ChromaFormat::Monochrome => "Mono",
        },
        image.is_null(),
        exif.is_null(),
    );

    let mut unowned = unsafe { EnvUnowned::from_raw(env) };

    let outcome = unowned.with_env(|env| -> Result<jobject, jni::errors::Error> {
        let result: Result<jobject, anyhow::Error> = (|| {
            let quality = quality.clamp(1, 100) as u32;
            dbg_log!(debug, "clamped quality={quality}");

            let cicp = resolve_cicp_maroontree(color_space);
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

            let encoded_data = encode_av2_inner(
                &mut bitmap_data,
                &AvEncodingConfig {
                    cicp,
                    quality,
                    lossless,
                    exif: exif_data.map(|x| x.to_vec()),
                    chroma: chroma_subsampling,
                    speed,
                },
            )
            .map_err(|x| {
                dbg_log!(error, "encode_av2_inner failed: {x:#}");
                x
            })?;
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
        })();

        match result {
            Ok(obj) => {
                dbg_log!(debug, "encode_avif_av2_file: success");
                Ok(obj)
            }
            Err(e) => {
                dbg_log!(error, "encode_avif_av2_file failed: {e:#}");
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
            dbg_log!(debug, "encode_avif_av2_file: success");
            v
        }
        Outcome::Err(_e) => {
            dbg_log!(error, "JNI error in with_env: {}", _e.to_string());
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
