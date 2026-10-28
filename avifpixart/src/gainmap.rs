//! Android/ISO gainmap interop. This module is compiled only for Android ARM.
use crate::WeaverPreferredColorConfig;
use crate::av1_encode_android::resolve_cicp_maroontree;
use crate::ffi::{BitmapData, BitmapPixelFormat, get_bitmap_data, software_bitmap};
use crate::support::{PackedImageBuffer, android_os_version};
use jni::objects::{JFloatArray, JObject, JValue};
use jni::sys::jobject;
use jni::{Env, jni_sig, jni_str};
use maroontree::{BitDepth, GainMap, GainMapFloats, IsoGainMap, PlanarImage};

const BRIDGE: &str = "com/radzivon/bartoshyk/avif/coder/GainmapSupport";

pub(crate) fn has_gainmap(env: &mut Env<'_>, image: jobject) -> anyhow::Result<bool> {
    if android_os_version() < 34 {
        return Ok(false);
    }
    let bitmap = unsafe { JObject::from_raw(env, image) };
    Ok(env
        .call_method(&bitmap, jni_str!("hasGainmap"), jni_sig!("()Z"), &[])?
        .z()?)
}

/// Optional auxiliary failures may fall back to encoding fresh base pixels.
/// Retrying the base also ensures unrelated bitmap/codec errors still propagate.
pub(crate) fn encode_with_gainmap<G, T>(
    env: &mut Env<'_>,
    image: jobject,
    bitmap_data: &mut BitmapData,
    ignore_errors: bool,
    convert: impl FnOnce(AndroidGainmap) -> anyhow::Result<G>,
    mut encode: impl FnMut(&mut BitmapData, Option<G>) -> anyhow::Result<T>,
) -> anyhow::Result<T> {
    let gain_map = read_gainmap(env, image).and_then(|map| map.map(convert).transpose());
    let gain_map = match gain_map {
        Ok(map) => map,
        Err(error) if ignore_errors => {
            // A Kotlin/JNI gainmap validation error can leave an exception
            // pending; clear it before attempting further JNI work.
            if let Some(exception) = env.exception_occurred() {
                env.exception_clear();
                if env.is_instance_of(&exception, jni_str!("java/lang/VirtualMachineError"))?
                    || env.is_instance_of(&exception, jni_str!("java/lang/ThreadDeath"))?
                {
                    env.throw(exception)?;
                    return Err(error);
                }
            }
            None
        }
        Err(error) => return Err(error),
    };
    let had_gainmap = gain_map.is_some();
    match encode(bitmap_data, gain_map) {
        Err(_) if ignore_errors && had_gainmap => {
            // Pixel conversions can mutate the working buffer (e.g. RGB565),
            // so a retry must start with the original Bitmap's pixels.
            bitmap_data.data = Vec::new();
            *bitmap_data = unsafe { get_bitmap_data(env, image) }?;
            encode(bitmap_data, None)
        }
        result => result,
    }
}

pub(crate) struct AndroidGainmap {
    pub pixels: Vec<[f32; 3]>,
    pub width: usize,
    pub height: usize,
    pub metadata: GainMapFloats,
    pub alternate_data_space: i32,
    pub high_bit_depth: bool,
}

pub(crate) fn read_gainmap(
    env: &mut Env<'_>,
    image: jobject,
) -> anyhow::Result<Option<AndroidGainmap>> {
    if android_os_version() < 34 {
        return Ok(None);
    }
    let bitmap = unsafe { JObject::from_raw(env, image) };
    let bridge = env.find_class(jni::strings::JNIString::from(BRIDGE))?;
    let values = env
        .call_static_method(
            &bridge,
            jni_str!("metadata"),
            jni_sig!("(Landroid/graphics/Bitmap;)[F"),
            &[JValue::Object(&bitmap)],
        )?
        .l()?;
    if values.is_null() {
        return Ok(None);
    }
    let array = unsafe { JFloatArray::from_raw(env, values.as_raw().cast()) };
    anyhow::ensure!(array.len(env)? == 19, "Invalid gainmap metadata size");
    let mut v = [0f32; 19];
    array.get_region(env, 0, &mut v)?;
    let metadata = GainMapFloats {
        min: v[0..3].try_into()?,
        max: v[3..6].try_into()?,
        gamma: v[6..9].try_into()?,
        base_offset: v[9..12].try_into()?,
        alternate_offset: v[12..15].try_into()?,
        base_hdr_headroom: v[15],
        alternate_hdr_headroom: v[16],
        use_base_color_space: v[17] != 0.,
    };
    IsoGainMap::from_floats(&metadata)?;
    let contents = env
        .call_static_method(
            &bridge,
            jni_str!("contents"),
            jni_sig!("(Landroid/graphics/Bitmap;)Landroid/graphics/Bitmap;"),
            &[JValue::Object(&bitmap)],
        )?
        .l()?;
    anyhow::ensure!(!contents.is_null(), "Missing gainmap pixels");
    let data = unsafe { get_bitmap_data(env, contents.as_raw()) }?;
    let high_bit_depth = matches!(
        data.format,
        BitmapPixelFormat::RgbaF16 | BitmapPixelFormat::Rgba1010102
    );
    let mut pixels = Vec::with_capacity(data.width * data.height);
    for px in data.data.chunks_exact(data.format.bytes_per_pixel()) {
        let rgb = match data.format {
            BitmapPixelFormat::A8 => [px[0] as f32 / 255.; 3],
            BitmapPixelFormat::Rgba8888 => std::array::from_fn(|c| px[c] as f32 / 255.),
            BitmapPixelFormat::Rgb565 => {
                let rgb = crate::cvt::rgb565_to_rgba8888(u16::from_ne_bytes(px.try_into()?));
                std::array::from_fn(|c| rgb[c] as f32 / 255.)
            }
            BitmapPixelFormat::Rgba1010102 => {
                let rgb = crate::cvt::ar30_to_rgba10_pixel(u32::from_ne_bytes(px.try_into()?));
                std::array::from_fn(|c| rgb[c] as f32 / 1023.)
            }
            BitmapPixelFormat::RgbaF16 => std::array::from_fn(|c| {
                f16::from_bits(u16::from_ne_bytes([px[c * 2], px[c * 2 + 1]])) as f32
            }),
        };
        anyhow::ensure!(
            rgb.iter().all(|v| v.is_finite() && (0. ..=1.).contains(v)),
            "Gainmap samples must be finite and normalized"
        );
        pixels.push(rgb);
    }
    Ok(Some(AndroidGainmap {
        pixels,
        width: data.width,
        height: data.height,
        metadata,
        alternate_data_space: v[18] as i32,
        high_bit_depth,
    }))
}

impl AndroidGainmap {
    pub(crate) fn to_avif(&self) -> anyhow::Result<GainMap> {
        let metadata = IsoGainMap::from_floats(&self.metadata)?;
        let mut map = if self.high_bit_depth {
            let rgb: Vec<u16> = self
                .pixels
                .iter()
                .flatten()
                .map(|v| (v * 4095.).round() as u16)
                .collect();
            GainMap::rgb16(
                PlanarImage::from_interleaved_rgb(self.width, self.height, BitDepth::Twelve, &rgb)?,
                metadata,
            )
        } else {
            let rgb: Vec<u8> = self
                .pixels
                .iter()
                .flatten()
                .map(|v| (v * 255.).round() as u8)
                .collect();
            GainMap::rgb8(
                PlanarImage::from_interleaved_rgb(self.width, self.height, BitDepth::Eight, &rgb)?,
                metadata,
            )
        };
        if !self.metadata.use_base_color_space {
            anyhow::ensure!(
                self.alternate_data_space >= 0,
                "Unsupported gainmap alternative primaries"
            );
            map = map.with_alternate_cicp(resolve_cicp_maroontree(self.alternate_data_space));
        }
        Ok(map)
    }

    pub(crate) fn to_heic(&self) -> anyhow::Result<hpvca::GainMap> {
        let m = &self.metadata;
        anyhow::ensure!(
            m.base_hdr_headroom == 0. && m.alternate_hdr_headroom > 0. && m.use_base_color_space,
            "HEIC gainmaps require an SDR base and base-image primaries; use AVIF for other gainmaps"
        );
        anyhow::ensure!(
            m.base_offset == [0.; 3] && m.alternate_offset == [0.; 3],
            "HEIC Apple gainmaps cannot preserve nonzero offsets; use AVIF"
        );
        anyhow::ensure!(
            (1..3)
                .all(|c| m.min[c] == m.min[0] && m.max[c] == m.max[0] && m.gamma[c] == m.gamma[0])
                && self.pixels.iter().all(|p| p[0] == p[1] && p[0] == p[2]),
            "HEIC Apple gainmaps require a single gain channel; use AVIF for RGB gainmaps"
        );
        let headroom = 2f32.powf(m.alternate_hdr_headroom);
        let mut pixels = Vec::with_capacity(self.pixels.len());
        for p in &self.pixels {
            let ratio = 2f32.powf(m.min[0] + (m.max[0] - m.min[0]) * p[0].powf(1. / m.gamma[0]));
            anyhow::ensure!(
                ratio >= 1. && ratio <= headroom * 1.00001,
                "HEIC gainmap ratios must lie between 1 and HDR headroom; use AVIF"
            );
            pixels.push((hpvca::apple_encode(ratio, headroom) * 255.).round() as u8);
        }
        Ok(hpvca::GainMap::gray8(
            pixels,
            self.width as u32,
            self.height as u32,
            headroom,
        ))
    }
}

pub(crate) fn attach_gainmap(
    env: &mut Env<'_>,
    bitmap: &JObject<'_>,
    contents: &JObject<'_>,
    values: &[f32; 19],
) -> anyhow::Result<()> {
    if android_os_version() < 34 {
        return Ok(());
    }
    let array = env.new_float_array(19)?;
    array.set_region(env, 0, values)?;
    let bridge = env.find_class(jni::strings::JNIString::from(BRIDGE))?;
    env.call_static_method(
        &bridge,
        jni_str!("attach"),
        jni_sig!("(Landroid/graphics/Bitmap;Landroid/graphics/Bitmap;[F)V"),
        &[
            JValue::Object(bitmap),
            JValue::Object(contents),
            JValue::Object(&array),
        ],
    )?;
    Ok(())
}

/// Convert Apple's Rec.709-coded linear gains to Android's log-coded gains.
pub(crate) fn apple_bitmap<'local>(
    env: &mut Env<'local>,
    gain: &hpvcd::GainMapFrame,
    parent: &hpvcd::DecodedYuv,
    scaled_width: i32,
    scaled_height: i32,
    scale_mode: crate::WeaveScaleMode,
) -> anyhow::Result<Option<(JObject<'local>, [f32; 19])>> {
    let Some(metadata) = gain
        .metadata
        .as_ref()
        .and_then(|v| std::str::from_utf8(v).ok())
    else {
        return Ok(None);
    };
    let Some(headroom) = apple_headroom(metadata) else {
        return Ok(None);
    };
    let decoded = hpvcd::DecodedYuv {
        planes: gain.planes.clone(),
        alpha: None,
        gain_map: None,
        bit_depth: gain.bit_depth,
        chroma: gain.chroma,
        color: gain.color.clone(),
        orientation: hpvcd::Orientation::Normal,
        clean_aperture: None,
        pixel_aspect_ratio: None,
        exif: None,
    };
    let packed = crate::heic_decode::pack_decoded_heic(&decoded)?;
    let (width, height, pixels): (_, _, Vec<f32>) = match packed {
        crate::heic_decode::PackedHeic::Regular(p) => (
            p.width,
            p.height,
            p.data
                .chunks_exact(4)
                .flat_map(|p| p[..3].iter().map(|&v| v as f32 / 255.))
                .collect(),
        ),
        crate::heic_decode::PackedHeic::HighBitDepth(p) => {
            let max = ((1u32 << p.bit_depth.bits()) - 1) as f32;
            (
                p.width,
                p.height,
                p.data
                    .chunks_exact(4)
                    .flat_map(|p| p[..3].iter().map(|&v| v as f32 / max))
                    .collect(),
            )
        }
    };
    let mut rgba = Vec::with_capacity(width * height * 4);
    let log_headroom = headroom.log2();
    for p in pixels.chunks_exact(3) {
        for &v in p {
            rgba.push(
                (hpvca::apple_decode(v, headroom).log2() / log_headroom * 4095.)
                    .round()
                    .clamp(0., 4095.) as u16,
            );
        }
        rgba.push(4095);
    }
    let (mut width, mut height) = (width, height);
    // The base clap is in coded coordinates. Crop the corresponding map region
    // before applying display orientation or a sampled center crop.
    if let Some(clap) = parent.clean_aperture.as_ref()
        && let Some((left, top, cw, ch)) =
            crate::image_info::clap_rect(parent.width(), parent.height(), clap)
    {
        let x0 = left * width / parent.width();
        let y0 = top * height / parent.height();
        let x1 = ((left + cw) * width).div_ceil(parent.width()).min(width);
        let y1 = ((top + ch) * height).div_ceil(parent.height()).min(height);
        let mut cropped = Vec::with_capacity((x1 - x0) * (y1 - y0) * 4);
        for y in y0..y1 {
            cropped.extend_from_slice(&rgba[(y * width + x0) * 4..(y * width + x1) * 4]);
        }
        rgba = cropped;
        width = x1 - x0;
        height = y1 - y0;
    }
    let orientation = if gain.orientation == hpvcd::Orientation::Normal {
        parent.orientation
    } else {
        gain.orientation
    };
    let (rgba, width, height) =
        crate::orientation::apply_orientation(&rgba, width, height, orientation);
    let scaled = crate::scaling::internal_scale_u16(
        std::borrow::Cow::Owned(rgba),
        width * 4,
        width as u32,
        height as u32,
        scaled_width,
        scaled_height,
        12,
        false,
        scale_mode,
    )?;
    let buffer = PackedImageBuffer {
        data: crate::cvt::pack_10_or_12_to_f16(
            scaled.buffer.borrow(),
            scaled.width,
            scaled.height,
            12,
        )?,
        width: scaled.width,
        height: scaled.height,
        format: WeaverPreferredColorConfig::RgbaF16,
    };
    let contents = software_bitmap(env, &buffer, None)?;
    let mut values = [0.; 19];
    values[3..6].fill(log_headroom);
    values[6..9].fill(1.);
    values[16] = log_headroom;
    values[17] = 1.;
    values[18] = -1.;
    Ok(Some((contents, values)))
}

fn apple_headroom(xmp: &str) -> Option<f32> {
    let tail = xmp.split_once("HDRGainMapHeadroom")?.1.trim_start();
    let value = if let Some(tail) = tail.strip_prefix('=') {
        let tail = tail.trim_start();
        let quote = tail.chars().next()?;
        if quote != '\'' && quote != '"' {
            return None;
        }
        tail[1..].split(quote).next()?
    } else {
        tail.strip_prefix('>')?.split('<').next()?
    };
    value
        .trim()
        .parse::<f32>()
        .ok()
        .filter(|v| v.is_finite() && *v > 1.)
}

#[cfg(test)]
mod tests {
    use super::apple_headroom;
    #[test]
    fn reads_apple_headroom_attributes_and_elements() {
        assert_eq!(
            apple_headroom("HDRGainMap:HDRGainMapHeadroom=\"4.5\""),
            Some(4.5)
        );
        assert_eq!(
            apple_headroom("<HDRGainMap:HDRGainMapHeadroom>2</HDRGainMap:HDRGainMapHeadroom>"),
            Some(2.)
        );
        assert_eq!(apple_headroom("HDRGainMapHeadroom='NaN'"), None);
    }
}
