//! Portable ISO gainmap rendering shared by native decoders. No Android APIs.
use crate::ffi_panic::catch_unwind_or;
use crate::scaling::{WeaveScaleMode, internal_scale_u8, internal_scale_u16};
use moxcms::{
    CicpColorPrimaries, CicpProfile, ColorProfile, Matrix3f, MatrixCoefficients,
    TransferCharacteristics,
};
use std::borrow::Cow;
use std::sync::OnceLock;

const SRGB_LUT_INTERVALS: usize = 65536;

fn srgb_lut() -> &'static [f32] {
    static LUT: OnceLock<Box<[f32]>> = OnceLock::new();
    LUT.get_or_init(|| {
        (0..=SRGB_LUT_INTERVALS)
            .map(|i| {
                TransferCharacteristics::Srgb.gamma(i as f64 / SRGB_LUT_INTERVALS as f64) as f32
            })
            .collect()
    })
}

#[inline]
fn encode_srgb(lut: &[f32], linear: f32, max: u32) -> u32 {
    let position = linear.clamp(0., 1.) * SRGB_LUT_INTERVALS as f32;
    let index = (position as usize).min(SRGB_LUT_INTERVALS - 1);
    let fraction = position - index as f32;
    // Interpolate before quantization to retain precision at 16-bit output,
    // especially in the steep sRGB shadow curve.
    let encoded = lut[index] + (lut[index + 1] - lut[index]) * fraction;
    (encoded * max as f32).round() as u32
}

fn gain_luts(metadata: &[f32; 19], weight: f32, bit_depth: u32) -> [Vec<f32>; 3] {
    let max = (1u32 << bit_depth) - 1;
    std::array::from_fn(|c| {
        let inverse_gamma = 1. / metadata[6 + c];
        let min = metadata[c];
        let range = metadata[3 + c] - min;
        (0..=max)
            .map(|sample| {
                let log_gain = min + range * (sample as f32 / max as f32).powf(inverse_gamma);
                (log_gain * weight).exp2()
            })
            .collect()
    })
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct GainmapRenderImage {
    pub pixels: *const u8,
    /// Bytes per row, including any padding.
    pub stride: usize,
    pub width: u32,
    pub height: u32,
    pub bit_depth: u32,
}

impl GainmapRenderImage {
    fn row_bytes(self) -> Option<usize> {
        if self.pixels.is_null()
            || self.width == 0
            || self.height == 0
            || !matches!(self.bit_depth, 8 | 10 | 12 | 16)
        {
            return None;
        }
        let row = (self.width as usize).checked_mul(4 * self.sample_bytes())?;
        (self.stride >= row).then_some(row)
    }

    fn sample_bytes(self) -> usize {
        if self.bit_depth > 8 { 2 } else { 1 }
    }

    unsafe fn bytes<'a>(self) -> Option<&'a [u8]> {
        let length = self
            .stride
            .checked_mul(self.height as usize - 1)?
            .checked_add(self.row_bytes()?)?;
        if length > isize::MAX as usize {
            return None;
        }
        Some(unsafe { std::slice::from_raw_parts(self.pixels, length) })
    }
}

fn profile(primaries: u16) -> Option<ColorProfile> {
    let primaries = u8::try_from(primaries)
        .ok()
        .and_then(|v| CicpColorPrimaries::try_from(v).ok())
        .unwrap_or(CicpColorPrimaries::Bt709);
    let primaries = match primaries {
        CicpColorPrimaries::Reserved | CicpColorPrimaries::Unspecified => CicpColorPrimaries::Bt709,
        v => v,
    };
    let mut profile = ColorProfile::new_srgb();
    profile.cicp = None;
    profile.update_rgb_colorimetry_from_cicp(CicpProfile {
        color_primaries: primaries,
        transfer_characteristics: TransferCharacteristics::Linear,
        matrix_coefficients: MatrixCoefficients::Identity,
        full_range: true,
    });
    profile.cicp.is_some().then_some(profile)
}

fn convert(matrix: Matrix3f, rgb: [f32; 3]) -> [f32; 3] {
    std::array::from_fn(|c| {
        matrix.v[c][0] * rgb[0] + matrix.v[c][1] * rgb[1] + matrix.v[c][2] * rgb[2]
    })
}

enum AlignedGain {
    U8(pic_scale::ImageStoreMut<'static, u8, 4>),
    U16(pic_scale::ImageStoreMut<'static, u16, 4>),
}

impl AlignedGain {
    unsafe fn new(image: GainmapRenderImage, width: u32, height: u32) -> Option<Self> {
        image.row_bytes()?;
        let bytes = unsafe { image.bytes()? };
        if image.bit_depth == 8 {
            Some(Self::U8(
                internal_scale_u8(
                    Cow::Borrowed(bytes),
                    image.stride.try_into().ok()?,
                    image.width,
                    image.height,
                    width.try_into().ok()?,
                    height.try_into().ok()?,
                    false,
                    WeaveScaleMode::JustResize,
                )
                .ok()?,
            ))
        } else {
            // Copy by rows so odd byte strides and unaligned inputs remain valid.
            let row_samples = (image.width as usize).checked_mul(4)?;
            let mut samples = vec![0u16; row_samples.checked_mul(image.height as usize)?];
            for (src, dst) in bytes
                .chunks(image.stride)
                .zip(samples.chunks_exact_mut(row_samples))
            {
                for (dst, src) in dst
                    .iter_mut()
                    .zip(src[..row_samples * 2].as_chunks::<2>().0)
                {
                    *dst = u16::from_ne_bytes(*src);
                }
            }
            Some(Self::U16(
                internal_scale_u16(
                    Cow::Owned(samples),
                    row_samples,
                    image.width,
                    image.height,
                    width.try_into().ok()?,
                    height.try_into().ok()?,
                    image.bit_depth as usize,
                    false,
                    WeaveScaleMode::JustResize,
                )
                .ok()?,
            ))
        }
    }
}

struct RenderRows<'a> {
    source: &'a [u8],
    source_stride: usize,
    destination: &'a mut [u8],
    destination_stride: usize,
    width: usize,
}

impl RenderRows<'_> {
    fn pixels<const BYTES: usize>(
        &mut self,
    ) -> impl Iterator<Item = (&[[u8; BYTES]], &mut [[u8; BYTES]])> {
        let row_bytes = self.width * BYTES;
        self.source
            .chunks(self.source_stride)
            .zip(self.destination.chunks_mut(self.destination_stride))
            .map(move |(src, dst)| {
                (
                    src[..row_bytes].as_chunks::<BYTES>().0,
                    dst[..row_bytes].as_chunks_mut::<BYTES>().0,
                )
            })
    }
}

struct RenderLuts<'a> {
    linear: &'a [f32],
    output_gamma: &'a [f32],
    max: u32,
}

impl RenderLuts<'_> {
    #[inline]
    fn decode<const BYTES: usize>(&self, source: &[u8; BYTES]) -> Option<[f32; 3]> {
        let mut rgb = [0.; 3];
        // BYTES is fixed at dispatch, so these branches disappear in each kernel.
        if BYTES == 4 {
            for (value, &sample) in rgb.iter_mut().zip(source) {
                *value = *self.linear.get(sample as usize)?;
            }
        } else {
            for (value, sample) in rgb.iter_mut().zip(source.as_chunks::<2>().0) {
                *value = *self.linear.get(u16::from_ne_bytes(*sample) as usize)?;
            }
        }
        Some(rgb)
    }

    #[inline]
    fn encode<const BYTES: usize>(
        &self,
        rgb: [f32; 3],
        source: &[u8; BYTES],
        destination: &mut [u8; BYTES],
    ) -> Option<()> {
        let sample_bytes = BYTES / 4;
        let alpha_offset = sample_bytes * 3;
        for (value, dst) in rgb
            .into_iter()
            .zip(destination[..alpha_offset].chunks_exact_mut(sample_bytes))
        {
            if !value.is_finite() {
                return None;
            }
            let encoded = encode_srgb(self.output_gamma, value, self.max);
            if BYTES == 4 {
                dst[0] = encoded as u8;
            } else {
                dst.copy_from_slice(&(encoded as u16).to_ne_bytes());
            }
        }
        destination[alpha_offset..].copy_from_slice(&source[alpha_offset..]);
        Some(())
    }
}

struct GainChannel<'a> {
    factors: &'a [f32],
    base_offset: f32,
    alternate_offset: f32,
}

struct GainTransform<'a> {
    channels: [GainChannel<'a>; 3],
    max: usize,
    input_matrix: Matrix3f,
    output_matrix: Matrix3f,
}

impl GainTransform<'_> {
    fn render<const BYTES: usize, T: Copy + Into<usize>>(
        &self,
        rows: &mut RenderRows<'_>,
        luts: &RenderLuts<'_>,
        gain: &[T],
        gain_stride: usize,
    ) -> Option<()> {
        let row_samples = rows.width * 4;
        for ((src_row, dst_row), gain_row) in rows.pixels::<BYTES>().zip(gain.chunks(gain_stride)) {
            for ((src, dst), gain_pixel) in src_row
                .iter()
                .zip(dst_row.iter_mut())
                .zip(gain_row[..row_samples].as_chunks::<4>().0)
            {
                let mut rgb = convert(self.input_matrix, luts.decode(src)?);
                for ((value, &sample), channel) in
                    rgb.iter_mut().zip(gain_pixel).zip(&self.channels)
                {
                    *value = (*value + channel.base_offset)
                        * channel.factors[sample.into().min(self.max)]
                        - channel.alternate_offset;
                }
                luts.encode(convert(self.output_matrix, rgb), src, dst)?;
            }
        }
        Some(())
    }
}

fn render_pixels<const BYTES: usize>(
    rows: &mut RenderRows<'_>,
    luts: &RenderLuts<'_>,
    aligned: &AlignedGain,
    gain: Option<&GainTransform<'_>>,
    base_output_matrix: Matrix3f,
) -> Option<()> {
    // Select gain mode and gain sample type once, outside the row/pixel loops.
    if let Some(gain) = gain {
        match aligned {
            AlignedGain::U8(image) => {
                gain.render::<BYTES, _>(rows, luts, image.buffer.borrow(), image.stride())
            }
            AlignedGain::U16(image) => {
                gain.render::<BYTES, _>(rows, luts, image.buffer.borrow(), image.stride())
            }
        }
    } else {
        // A zero weight bypasses both offsets and the gain math color space.
        for (src_row, dst_row) in rows.pixels::<BYTES>() {
            for (src, dst) in src_row.iter().zip(dst_row.iter_mut()) {
                luts.encode(convert(base_output_matrix, luts.decode(src)?), src, dst)?;
            }
        }
        Some(())
    }
}

/// Render zero display headroom (SDR) to RGBA at the base image's bit depth.
/// Metadata uses the same 19-float ISO layout as Android gainmap interop.
/// All image buffers must be valid and output must not overlap either input.
/// Returns false on invalid parameters or a caught panic; discard output then.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn weave_render_gainmap_sdr(
    base: GainmapRenderImage,
    gain: GainmapRenderImage,
    metadata: *const f32,
    base_primaries: u16,
    base_transfer: u16,
    alternate_primaries: u16,
    output: *mut u8,
    output_stride: usize,
) -> bool {
    catch_unwind_or(false, || unsafe {
        render(
            base,
            gain,
            metadata,
            base_primaries,
            base_transfer,
            alternate_primaries,
            output,
            output_stride,
        )
        .is_some()
    })
}

#[allow(clippy::too_many_arguments)]
unsafe fn render(
    base: GainmapRenderImage,
    gain: GainmapRenderImage,
    metadata: *const f32,
    base_primaries: u16,
    base_transfer: u16,
    alternate_primaries: u16,
    output: *mut u8,
    output_stride: usize,
) -> Option<()> {
    let row = base.row_bytes()?;
    gain.row_bytes()?;
    if metadata.is_null() || output.is_null() || output_stride < row {
        return None;
    }
    let output_length = output_stride
        .checked_mul(base.height as usize - 1)?
        .checked_add(row)?;
    if output_length > isize::MAX as usize {
        return None;
    }
    let m = unsafe { std::ptr::read_unaligned(metadata.cast::<[f32; 19]>()) };
    if !m.iter().all(|v| v.is_finite())
        || m[15] < 0.
        || m[16] < 0.
        || (0..3).any(|c| m[c] > m[3 + c] || m[6 + c] <= 0.)
    {
        return None;
    }
    let weight = if m[15] == m[16] {
        0.
    } else {
        ((-m[15]) / (m[16] - m[15])).clamp(0., 1.) * (m[16] - m[15]).signum()
    };
    let base_profile = profile(base_primaries)?;
    let math_profile = if weight == 0. || m[17] != 0. || alternate_primaries == 2 {
        base_profile.clone()
    } else {
        profile(alternate_primaries)?
    };
    let input_matrix = base_profile.transform_matrix(&math_profile).to_f32();
    let output_matrix = math_profile
        .transform_matrix(&ColorProfile::new_srgb())
        .to_f32();
    let base_output_matrix = base_profile
        .transform_matrix(&ColorProfile::new_srgb())
        .to_f32();
    let transfer = match u8::try_from(base_transfer)
        .ok()
        .and_then(|v| TransferCharacteristics::try_from(v).ok())
        .unwrap_or(TransferCharacteristics::Bt709)
    {
        TransferCharacteristics::Reserved | TransferCharacteristics::Unspecified => {
            TransferCharacteristics::Bt709
        }
        v => v,
    };
    // moxcms normalizes PQ/HLG to peak white. ISO gain math uses 203-nit SDR white.
    let white_scale = match transfer {
        TransferCharacteristics::Smpte2084 => 10000. / 203.,
        TransferCharacteristics::Hlg => 1000. / 203.,
        _ => 1.,
    };
    let max = (1u32 << base.bit_depth) - 1;
    let linear: Vec<f32> = (0..=max)
        .map(|v| (transfer.linearize(v as f64 / max as f64) * white_scale) as f32)
        .collect();
    // Each scaled gain sample is an integer code value. Tabulate its complete
    // gamma/log-gain/exponent calculation once, separately for each channel.
    let gain_factors = (weight != 0.).then(|| gain_luts(&m, weight, gain.bit_depth));
    let gain_transform = gain_factors.as_ref().map(|factors| GainTransform {
        channels: std::array::from_fn(|c| GainChannel {
            factors: &factors[c],
            base_offset: m[9 + c],
            alternate_offset: m[12 + c],
        }),
        max: ((1u32 << gain.bit_depth) - 1) as usize,
        input_matrix,
        output_matrix,
    });
    let luts = RenderLuts {
        linear: &linear,
        output_gamma: srgb_lut(),
        max,
    };
    let bytes = unsafe { base.bytes()? };
    let aligned = unsafe { AlignedGain::new(gain, base.width, base.height)? };
    let destination = unsafe { std::slice::from_raw_parts_mut(output, output_length) };
    let mut rows = RenderRows {
        source: bytes,
        source_stride: base.stride,
        destination,
        destination_stride: output_stride,
        width: base.width as usize,
    };
    if base.bit_depth == 8 {
        render_pixels::<4>(
            &mut rows,
            &luts,
            &aligned,
            gain_transform.as_ref(),
            base_output_matrix,
        )
    } else {
        render_pixels::<8>(
            &mut rows,
            &luts,
            &aligned,
            gain_transform.as_ref(),
            base_output_matrix,
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn iterator_kernels_preserve_padding_and_unaligned_rows() {
        fn pixels(width: usize, depth: u32) -> Vec<u8> {
            let sample_bytes = if depth == 8 { 1 } else { 2 };
            let max = (1u32 << depth) - 1;
            let mut data = vec![0; width * 2 * 4 * sample_bytes];
            for (i, sample) in data.chunks_exact_mut(sample_bytes).enumerate() {
                let value = ((i as u32 * 1543 + max / 7) % (max + 1)) as u16;
                if depth == 8 {
                    sample[0] = value as u8;
                } else {
                    sample.copy_from_slice(&value.to_ne_bytes());
                }
            }
            data
        }

        fn pad(data: &[u8], row: usize, stride: usize) -> Vec<u8> {
            // No final-row padding: the renderer must also process a short last chunk.
            let mut padded = vec![0xa5; 1 + stride + row];
            for (dst, src) in padded[1..].chunks_mut(stride).zip(data.chunks_exact(row)) {
                dst[..row].copy_from_slice(src);
            }
            padded
        }

        for depth in [8, 10, 12, 16] {
            for gain_depth in [8, 10, 12, 16] {
                let source = pixels(3, depth);
                let gains = pixels(2, gain_depth);
                let row = source.len() / 2;
                let gain_row = gains.len() / 2;
                let source_stride = row + 3;
                let gain_stride = gain_row + 1;
                let output_stride = row + 5;
                let padded_source = pad(&source, row, source_stride);
                let padded_gain = pad(&gains, gain_row, gain_stride);
                let base = GainmapRenderImage {
                    pixels: source.as_ptr(),
                    stride: row,
                    width: 3,
                    height: 2,
                    bit_depth: depth,
                };
                let gain = GainmapRenderImage {
                    pixels: gains.as_ptr(),
                    stride: gain_row,
                    width: 2,
                    height: 2,
                    bit_depth: gain_depth,
                };
                let padded_base = GainmapRenderImage {
                    pixels: padded_source[1..].as_ptr(),
                    stride: source_stride,
                    ..base
                };
                let padded_gain = GainmapRenderImage {
                    pixels: padded_gain[1..].as_ptr(),
                    stride: gain_stride,
                    ..gain
                };
                for apply_gain in [false, true] {
                    let mut metadata = [0.; 19];
                    metadata[3..6].fill(2.);
                    metadata[6..9].copy_from_slice(&[0.75, 1.25, 2.]);
                    metadata[9..12].fill(0.05);
                    metadata[12..15].fill(-0.025);
                    metadata[if apply_gain { 15 } else { 16 }] = 2.;
                    metadata[17] = 1.;
                    let mut expected = vec![0; source.len()];
                    let mut output = vec![0xd3; 1 + output_stride * 2];
                    assert!(unsafe {
                        weave_render_gainmap_sdr(
                            base,
                            gain,
                            metadata.as_ptr(),
                            1,
                            13,
                            9,
                            expected.as_mut_ptr(),
                            row,
                        )
                    });
                    assert!(unsafe {
                        weave_render_gainmap_sdr(
                            padded_base,
                            padded_gain,
                            metadata.as_ptr(),
                            1,
                            13,
                            9,
                            output[1..].as_mut_ptr(),
                            output_stride,
                        )
                    });
                    assert_eq!(output[0], 0xd3);
                    for (actual, expected) in output[1..]
                        .chunks(output_stride)
                        .zip(expected.chunks_exact(row))
                    {
                        assert_eq!(&actual[..row], expected);
                        assert!(actual[row..].iter().all(|&v| v == 0xd3));
                    }
                }
            }
        }
    }

    #[test]
    fn interpolated_srgb_lut_preserves_output_precision() {
        let lut = srgb_lut();
        for depth in [8, 10, 12, 16] {
            let max = (1u32 << depth) - 1;
            // Include table midpoints, where interpolation error is largest.
            for i in 0..=SRGB_LUT_INTERVALS * 2 {
                let linear = i as f32 / (SRGB_LUT_INTERVALS * 2) as f32;
                let expected = (TransferCharacteristics::Srgb.gamma(linear as f64) * max as f64)
                    .round() as u32;
                assert!(
                    encode_srgb(lut, linear, max).abs_diff(expected) <= 1,
                    "sRGB LUT error at depth={depth}, linear={linear}"
                );
            }
            for linear in [-1., 0., 0.0031308, 1., 2.] {
                let expected = (TransferCharacteristics::Srgb
                    .gamma(linear as f64)
                    .clamp(0., 1.)
                    * max as f64)
                    .round() as u32;
                assert!(encode_srgb(lut, linear, max).abs_diff(expected) <= 1);
            }
        }
    }

    #[test]
    fn zero_weight_bypasses_nonzero_offsets_and_alternate_primaries() {
        let source = [128, 64, 32, 87];
        let gain_pixels = [200, 100, 50, 255];
        let image = GainmapRenderImage {
            pixels: source.as_ptr(),
            stride: 4,
            width: 1,
            height: 1,
            bit_depth: 8,
        };
        let gain = GainmapRenderImage {
            pixels: gain_pixels.as_ptr(),
            ..image
        };
        let mut metadata = [0.; 19];
        metadata[3..6].fill(2.);
        metadata[6..9].fill(0.5);
        metadata[9..12].fill(0.25);
        metadata[12..15].fill(-0.125);
        metadata[16] = 2.;
        let mut output = [0u8; 4];
        assert!(unsafe {
            weave_render_gainmap_sdr(
                image,
                gain,
                metadata.as_ptr(),
                1,
                13,
                9,
                output.as_mut_ptr(),
                4,
            )
        });
        assert_eq!(output, source);
    }

    #[test]
    fn invalid_metadata_and_layout_return_failure() {
        let pixels = [128, 64, 32, 255];
        let image = GainmapRenderImage {
            pixels: pixels.as_ptr(),
            stride: 4,
            width: 1,
            height: 1,
            bit_depth: 8,
        };
        let mut metadata = [0.; 19]; // Zero gamma is invalid.
        let mut output = [17u8; 4];
        assert!(!unsafe {
            weave_render_gainmap_sdr(
                image,
                image,
                metadata.as_ptr(),
                1,
                13,
                9,
                output.as_mut_ptr(),
                4,
            )
        });
        assert_eq!(output, [17; 4]);
        metadata[6..9].fill(1.);
        let invalid = GainmapRenderImage { stride: 3, ..image };
        assert!(!unsafe {
            weave_render_gainmap_sdr(
                invalid,
                image,
                metadata.as_ptr(),
                1,
                13,
                9,
                output.as_mut_ptr(),
                4,
            )
        });
    }
}
