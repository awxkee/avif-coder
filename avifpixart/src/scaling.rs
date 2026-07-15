/*
 * Copyright (c) Radzivon Bartoshyk 5/2026. All rights reserved.
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

use core::f16;
use pic_scale::{
    BufferStore, ImageStore, ImageStoreMut, PicScaleError, ResamplingFunction, Scaler,
    ThreadingPolicy, WorkloadStrategy,
};
use std::slice;

#[repr(C)]
pub struct ScalingResult {
    data: *mut u8,
    width: usize,
    height: usize,
    stride: usize,
    length: usize,
    capacity: usize,
}

#[repr(C)]
pub struct ScalingResultU16 {
    data: *mut u16,
    width: usize,
    height: usize,
    stride: usize,
    length: usize,
    capacity: usize,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Ord, PartialOrd, Eq)]
pub enum WeaveScaleMode {
    JustResize,
    ScaleToFill,
    ScaleToFit,
}

#[unsafe(no_mangle)]
pub extern "C" fn weave_scaling_result_free(result: ScalingResult) {
    if result.data.is_null() {
        return;
    }
    unsafe {
        _ = Vec::from_raw_parts(result.data, result.length, result.capacity);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn weave_scaling_result16_free(result: ScalingResultU16) {
    if result.data.is_null() {
        return;
    }
    unsafe {
        _ = Vec::from_raw_parts(result.data, result.length, result.capacity);
    }
}

#[inline]
fn round_up_to_even_dimension(value: usize) -> usize {
    if value <= 1 {
        2
    } else {
        value.saturating_add(value & 1)
    }
}

#[inline]
fn rounded_ratio(numerator: u128, denominator: u128) -> usize {
    debug_assert!(denominator != 0);
    ((numerator + denominator / 2) / denominator) as usize
}

/// Returns the centered source crop whose aspect ratio most closely matches the
/// requested output. Cropping before resampling avoids allocating a potentially
/// enormous cover-scaled intermediate for very wide or very tall images.
fn scale_to_fill_crop(
    width: usize,
    height: usize,
    target_width: usize,
    target_height: usize,
) -> (usize, usize, usize, usize) {
    let source_ratio = width as u128 * target_height as u128;
    let target_ratio = target_width as u128 * height as u128;

    if source_ratio > target_ratio {
        let crop_width =
            rounded_ratio(height as u128 * target_width as u128, target_height as u128)
                .clamp(1, width);
        ((width - crop_width) / 2, 0, crop_width, height)
    } else if source_ratio < target_ratio {
        let crop_height =
            rounded_ratio(width as u128 * target_height as u128, target_width as u128)
                .clamp(1, height);
        (0, (height - crop_height) / 2, width, crop_height)
    } else {
        (0, 0, width, height)
    }
}

fn scale_to_fit_dimensions(
    width: usize,
    height: usize,
    target_width: usize,
    target_height: usize,
) -> (usize, usize) {
    if target_width as u128 * height as u128 <= target_height as u128 * width as u128 {
        let scaled_height = rounded_ratio(height as u128 * target_width as u128, width as u128)
            .clamp(1, target_height);
        (target_width, scaled_height)
    } else {
        let scaled_width = rounded_ratio(width as u128 * target_height as u128, height as u128)
            .clamp(1, target_width);
        (scaled_width, target_height)
    }
}

struct CropView<'a, T: Clone>
where
    [T]: ToOwned<Owned = Vec<T>>,
{
    data: std::borrow::Cow<'a, [T]>,
    stride: usize,
    width: usize,
    height: usize,
}

fn crop_rgba_for_scale_to_fill<T: Copy>(
    src: std::borrow::Cow<'_, [T]>,
    src_stride: usize,
    width: usize,
    height: usize,
    target_width: usize,
    target_height: usize,
) -> Result<CropView<'_, T>, PicScaleError> {
    const CHANNELS: usize = 4;

    let (crop_x, crop_y, crop_width, crop_height) =
        scale_to_fill_crop(width, height, target_width, target_height);
    if crop_x == 0 && crop_y == 0 && crop_width == width && crop_height == height {
        return Ok(CropView {
            data: src.clone(),
            stride: src_stride,
            width,
            height,
        });
    }

    let source_row = width.checked_mul(CHANNELS).ok_or_else(|| {
        PicScaleError::Generic(format!("RGBA source row overflow for width {width}"))
    })?;
    if src_stride < source_row {
        return Err(PicScaleError::Generic(format!(
            "RGBA source stride {src_stride} is smaller than row width {source_row}"
        )));
    }

    let crop_row = crop_width.checked_mul(CHANNELS).ok_or_else(|| {
        PicScaleError::Generic(format!("RGBA crop row overflow for width {crop_width}"))
    })?;
    let crop_len = crop_row.checked_mul(crop_height).ok_or_else(|| {
        PicScaleError::Generic(format!(
            "RGBA crop length overflow for {crop_width}x{crop_height}"
        ))
    })?;
    let x_offset = crop_x.checked_mul(CHANNELS).ok_or_else(|| {
        PicScaleError::Generic(format!("RGBA crop offset overflow for x={crop_x}"))
    })?;

    let row_range = |row: usize,
                     source_len: usize|
     -> Result<std::ops::Range<usize>, PicScaleError> {
        let start = row
            .checked_mul(src_stride)
            .and_then(|offset| offset.checked_add(x_offset))
            .ok_or_else(|| PicScaleError::Generic("RGBA crop offset overflow".to_string()))?;
        let end = start
            .checked_add(crop_row)
            .ok_or_else(|| PicScaleError::Generic("RGBA crop end overflow".to_string()))?;
        if end > source_len {
            return Err(PicScaleError::Generic(format!(
                "RGBA source is too short for crop: len={source_len}, row={row}, range={start}..{end}"
            )));
        }
        Ok(start..end)
    };

    let cropped = match src {
        std::borrow::Cow::Owned(mut source) => {
            // Decoder output is tightly packed and owned. Compact the centered
            // crop in place, avoiding a second full-resolution allocation.
            for dst_row in 0..crop_height {
                let src_row = crop_y + dst_row;
                let range = row_range(src_row, source.len())?;
                let dst_start = dst_row * crop_row;
                source.copy_within(range, dst_start);
            }
            source.truncate(crop_len);
            source
        }
        std::borrow::Cow::Borrowed(source) => {
            let mut cropped = Vec::new();
            cropped.try_reserve_exact(crop_len).map_err(|_| {
                PicScaleError::Generic(format!(
                    "Can't allocate RGBA source crop of {crop_len} samples"
                ))
            })?;
            for row in crop_y..crop_y + crop_height {
                let range = row_range(row, source.len())?;
                cropped.extend_from_slice(&source[range]);
            }
            cropped
        }
    };

    Ok(CropView {
        data: std::borrow::Cow::Owned(cropped),
        stride: crop_row,
        width: crop_width,
        height: crop_height,
    })
}

fn resolve_dimensions(
    src_width: u32,
    src_height: u32,
    new_width: i32,
    new_height: i32,
) -> (usize, usize) {
    let src_width = src_width.max(1) as usize;
    let src_height = src_height.max(1) as usize;

    match (new_width, new_height) {
        // Original size, but force the scaler target to even dimensions.
        (-2, -2) => (
            round_up_to_even_dimension(src_width),
            round_up_to_even_dimension(src_height),
        ),
        // Android/JNI callers use a non-positive pair for "original size" / no resize.
        (w, h) if w <= 0 && h <= 0 => (src_width, src_height),
        (w, -1) if w > 0 => {
            // auto height
            let scale = w as f64 / src_width as f64;
            let h = (src_height as f64 * scale).round() as usize;
            (w as usize, h.max(1))
        }
        (w, -2) if w > 0 => {
            // auto height divisible by 2
            let scale = w as f64 / src_width as f64;
            let h = (src_height as f64 * scale).round() as usize;
            (w as usize, round_up_to_even_dimension(h))
        }
        (-1, h) if h > 0 => {
            // auto width
            let scale = h as f64 / src_height as f64;
            let w = (src_width as f64 * scale).round() as usize;
            (w.max(1), h as usize)
        }
        (-2, h) if h > 0 => {
            // auto width divisible by 2
            let scale = h as f64 / src_height as f64;
            let w = (src_width as f64 * scale).round() as usize;
            (round_up_to_even_dimension(w), h as usize)
        }
        (w, h) => {
            // both explicit
            (w.max(1) as usize, h.max(1) as usize)
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn weave_scale_u8(
    src: *const u8,
    src_stride: u32,
    width: u32,
    height: u32,
    new_width: i32,
    new_height: i32,
    premultiply_alpha: bool,
    scale_mode: WeaveScaleMode,
) -> ScalingResult {
    let origin_slice = unsafe { slice::from_raw_parts(src, src_stride as usize * height as usize) };

    let final_store = match internal_scale_u8(
        std::borrow::Cow::Borrowed(origin_slice),
        src_stride,
        width,
        height,
        new_width,
        new_height,
        premultiply_alpha,
        scale_mode,
    ) {
        Ok(v) => v,
        Err(_) => {
            return ScalingResult {
                data: std::ptr::null_mut(),
                width: 0,
                height: 0,
                stride: 0,
                length: 0,
                capacity: 0,
            };
        }
    };

    let final_width = final_store.width;
    let final_height = final_store.height;

    let stride = final_store.stride();

    let mut final_buffer = match final_store.buffer {
        BufferStore::Borrowed(v) => v.to_vec(),
        BufferStore::Owned(v) => v,
    };

    let data = final_buffer.as_mut_ptr();
    let capacity = final_buffer.capacity();
    let length = final_buffer.len();
    std::mem::forget(final_buffer);

    ScalingResult {
        data,
        capacity,
        length,
        stride,
        width: final_width,
        height: final_height,
    }
}

#[repr(C)]
#[derive(Copy, Clone)]
pub enum ScalingFunction {
    Lanczos3,
    Nearest,
    Bilinear,
}

impl From<u32> for ScalingFunction {
    fn from(value: u32) -> Self {
        if value == 3 {
            ScalingFunction::Lanczos3
        } else if value == 1 {
            ScalingFunction::Nearest
        } else {
            ScalingFunction::Bilinear
        }
    }
}

#[allow(clippy::too_many_arguments)]
pub(crate) fn internal_scale_u8(
    src: std::borrow::Cow<[u8]>,
    src_stride: u32,
    width: u32,
    height: u32,
    new_width: i32,
    new_height: i32,
    premultiply_alpha: bool,
    scale_mode: WeaveScaleMode,
) -> Result<ImageStoreMut<'static, u8, 4>, PicScaleError> {
    if width == 0 || height == 0 {
        return Err(PicScaleError::Generic(format!(
            "RGBA source dimensions must be non-zero, got {width}x{height}"
        )));
    }

    let scaler = Scaler::new(ResamplingFunction::MitchellNetravalli)
        .set_threading_policy(ThreadingPolicy::Single);

    let (target_width, target_height) = resolve_dimensions(width, height, new_width, new_height);
    let (src, src_stride, source_width, source_height, scale_width, scale_height) = match scale_mode
    {
        WeaveScaleMode::ScaleToFill => {
            let view = crop_rgba_for_scale_to_fill(
                src,
                src_stride as usize,
                width as usize,
                height as usize,
                target_width,
                target_height,
            )?;
            (
                view.data,
                view.stride,
                view.width,
                view.height,
                target_width,
                target_height,
            )
        }
        WeaveScaleMode::ScaleToFit => {
            let (scale_width, scale_height) = scale_to_fit_dimensions(
                width as usize,
                height as usize,
                target_width,
                target_height,
            );
            (
                src,
                src_stride as usize,
                width as usize,
                height as usize,
                scale_width,
                scale_height,
            )
        }
        WeaveScaleMode::JustResize => (
            src,
            src_stride as usize,
            width as usize,
            height as usize,
            target_width,
            target_height,
        ),
    };

    let source_store = ImageStore::<u8, 4> {
        buffer: src,
        channels: 4,
        width: source_width,
        height: source_height,
        stride: src_stride,
        bit_depth: 8,
    };
    let mut scaled_store = ImageStoreMut::try_alloc(scale_width, scale_height)?;

    let plan =
        scaler.plan_rgba_resampling(source_store.size(), scaled_store.size(), premultiply_alpha)?;
    plan.resample(&source_store, &mut scaled_store)?;
    Ok(scaled_store)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn weave_scale_u16(
    src: *const u16,
    src_stride: usize,
    width: u32,
    height: u32,
    new_width: i32,
    new_height: i32,
    bit_depth: usize,
    premultiply_alpha: bool,
    scale_mode: WeaveScaleMode,
) -> ScalingResultU16 {
    let source_image: std::borrow::Cow<[u16]>;
    let mut j_src_stride = src_stride / 2;

    if !(src as usize).is_multiple_of(2) || !src_stride.is_multiple_of(2) {
        let mut _src_slice = vec![0u16; width as usize * height as usize * 4];
        let j = unsafe { slice::from_raw_parts(src as *const u8, src_stride * height as usize) };

        for (dst, src) in _src_slice
            .chunks_exact_mut(width as usize * 4)
            .zip(j.chunks_exact(src_stride))
        {
            for (dst, src) in dst.iter_mut().zip(src.as_chunks::<2>().0.iter()) {
                let pixel = u16::from_ne_bytes([src[0], src[1]]);
                *dst = pixel;
            }
        }
        source_image = std::borrow::Cow::Owned(_src_slice);
        j_src_stride = width as usize * 4;
    } else {
        unsafe {
            source_image = std::borrow::Cow::Borrowed(slice::from_raw_parts(
                src,
                src_stride / 2 * height as usize,
            ));
        }
    }

    let final_store = match internal_scale_u16(
        source_image,
        j_src_stride,
        width,
        height,
        new_width,
        new_height,
        bit_depth,
        premultiply_alpha,
        scale_mode,
    ) {
        Ok(v) => v,
        Err(_) => {
            return ScalingResultU16 {
                data: std::ptr::null_mut(),
                width: 0,
                height: 0,
                stride: 0,
                capacity: 0,
                length: 0,
            };
        }
    };

    let final_width = final_store.width;
    let final_height = final_store.height;

    let stride = final_store.stride();

    let mut final_buffer = match final_store.buffer {
        BufferStore::Borrowed(v) => v.to_vec(),
        BufferStore::Owned(v) => v,
    };

    let data = final_buffer.as_mut_ptr();
    let capacity = final_buffer.capacity();
    let length = final_buffer.len();
    std::mem::forget(final_buffer);

    ScalingResultU16 {
        data,
        capacity,
        length,
        stride,
        width: final_width,
        height: final_height,
    }
}

#[allow(clippy::too_many_arguments)]
pub(crate) fn internal_scale_u16(
    src: std::borrow::Cow<'_, [u16]>,
    src_stride: usize,
    width: u32,
    height: u32,
    new_width: i32,
    new_height: i32,
    bit_depth: usize,
    premultiply_alpha: bool,
    scale_mode: WeaveScaleMode,
) -> Result<ImageStoreMut<'static, u16, 4>, PicScaleError> {
    if width == 0 || height == 0 {
        return Err(PicScaleError::Generic(format!(
            "RGBA source dimensions must be non-zero, got {width}x{height}"
        )));
    }

    let scaler = Scaler::new(ResamplingFunction::MitchellNetravalli)
        .set_threading_policy(ThreadingPolicy::Single)
        .set_workload_strategy(WorkloadStrategy::PreferQuality);

    let (target_width, target_height) = resolve_dimensions(width, height, new_width, new_height);
    let (src, src_stride, source_width, source_height, scale_width, scale_height) = match scale_mode
    {
        WeaveScaleMode::ScaleToFill => {
            let view = crop_rgba_for_scale_to_fill(
                src,
                src_stride,
                width as usize,
                height as usize,
                target_width,
                target_height,
            )?;
            (
                view.data,
                view.stride,
                view.width,
                view.height,
                target_width,
                target_height,
            )
        }
        WeaveScaleMode::ScaleToFit => {
            let (scale_width, scale_height) = scale_to_fit_dimensions(
                width as usize,
                height as usize,
                target_width,
                target_height,
            );
            (
                src,
                src_stride,
                width as usize,
                height as usize,
                scale_width,
                scale_height,
            )
        }
        WeaveScaleMode::JustResize => (
            src,
            src_stride,
            width as usize,
            height as usize,
            target_width,
            target_height,
        ),
    };

    let source_store = ImageStore::<u16, 4> {
        buffer: src,
        channels: 4,
        width: source_width,
        height: source_height,
        stride: src_stride,
        bit_depth,
    };

    let Ok(mut scaled_store) =
        ImageStoreMut::try_alloc_with_depth(scale_width, scale_height, bit_depth)
    else {
        return Err(PicScaleError::Generic(
            "Can't allocate required buffer".to_string(),
        ));
    };

    let plan = scaler.plan_rgba_resampling16(
        source_store.size(),
        scaled_store.size(),
        premultiply_alpha,
        bit_depth,
    )?;
    plan.resample(&source_store, &mut scaled_store)?;
    Ok(scaled_store)
}

#[unsafe(no_mangle)]
pub extern "C" fn weave_scale_f16(
    src: *const u16,
    src_stride: usize,
    width: u32,
    height: u32,
    dst: *mut u16,
    new_width: u32,
    new_height: u32,
    method: u32,
    premultiply_alpha: bool,
) {
    unsafe {
        let source_image: std::borrow::Cow<[f16]>;
        let mut j_src_stride = src_stride / 2;

        if !(src as usize).is_multiple_of(2) || !src_stride.is_multiple_of(2) {
            let mut _src_slice: Vec<f16> = vec![0.; width as usize * height as usize * 4];
            let j = slice::from_raw_parts(src as *const u8, src_stride * height as usize);

            for (dst, src) in _src_slice
                .chunks_exact_mut(width as usize * 4)
                .zip(j.chunks_exact(src_stride))
            {
                for (dst, src) in dst.iter_mut().zip(src.as_chunks::<2>().0.iter()) {
                    let pixel = f16::from_bits(u16::from_ne_bytes([src[0], src[1]]));
                    *dst = pixel;
                }
            }
            source_image = std::borrow::Cow::Owned(_src_slice);
            j_src_stride = width as usize * 4;
        } else {
            source_image = std::borrow::Cow::Borrowed(slice::from_raw_parts(
                src as *const _,
                src_stride / 2 * height as usize,
            ));
        }

        let _source_store = ImageStore::<f16, 4> {
            buffer: source_image,
            channels: 4,
            width: width as usize,
            height: height as usize,
            stride: j_src_stride,
            bit_depth: 10,
        };

        let scaler = Scaler::new(if method == 3 {
            ResamplingFunction::Lanczos3
        } else if method == 1 {
            ResamplingFunction::Nearest
        } else {
            ResamplingFunction::Bilinear
        })
        .set_threading_policy(ThreadingPolicy::Single)
        .set_workload_strategy(WorkloadStrategy::PreferQuality);

        if !(dst as usize).is_multiple_of(2) {
            let mut dst_store =
                ImageStoreMut::alloc_with_depth(new_width as usize, new_height as usize, 10);

            let plan = match scaler.plan_rgba_resampling_f16(
                _source_store.size(),
                dst_store.size(),
                premultiply_alpha,
            ) {
                Ok(v) => v,
                Err(_) => return,
            };
            _ = plan.resample(&_source_store, &mut dst_store);

            let dst_slice = slice::from_raw_parts_mut(
                dst as *mut u8,
                new_width as usize * 4 * new_height as usize,
            );

            for (src, dst) in dst_store
                .as_bytes()
                .chunks_exact(dst_store.stride())
                .zip(dst_slice.chunks_exact_mut(new_width as usize * 4))
            {
                for (src, dst) in src.iter().zip(dst.as_chunks_mut::<2>().0.iter_mut()) {
                    let dst_ptr = dst.as_mut_ptr() as *mut f16;
                    dst_ptr.write_unaligned(*src);
                }
            }
        } else {
            let dst_stride = std::slice::from_raw_parts_mut(
                dst as *mut _,
                new_height as usize * new_width as usize * 4,
            );
            let buffer = BufferStore::Borrowed(dst_stride);
            let mut dst_store = ImageStoreMut::<f16, 4> {
                buffer,
                width: new_width as usize,
                height: new_height as usize,
                bit_depth: 10,
                channels: 4,
                stride: new_width as usize * 4,
            };

            let plan = match scaler.plan_rgba_resampling_f16(
                _source_store.size(),
                dst_store.size(),
                premultiply_alpha,
            ) {
                Ok(v) => v,
                Err(_) => return,
            };
            _ = plan.resample(&_source_store, &mut dst_store);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::resolve_dimensions;

    #[test]
    fn resolve_dimensions_keeps_original_for_zero_pair() {
        assert_eq!(resolve_dimensions(4032, 3024, 0, 0), (4032, 3024));
    }

    #[test]
    fn resolve_dimensions_keeps_original_for_negative_one_pair() {
        assert_eq!(resolve_dimensions(4032, 3024, -1, -1), (4032, 3024));
    }

    #[test]
    fn resolve_dimensions_rounds_original_to_even_for_negative_two_pair() {
        assert_eq!(resolve_dimensions(4032, 3024, -2, -2), (4032, 3024));
        assert_eq!(resolve_dimensions(4033, 3025, -2, -2), (4034, 3026));
    }

    #[test]
    fn resolve_dimensions_preserves_auto_single_axis_modes() {
        assert_eq!(resolve_dimensions(4000, 2000, 1000, -1), (1000, 500));
        assert_eq!(resolve_dimensions(4000, 2000, -1, 500), (1000, 500));
        assert_eq!(resolve_dimensions(4000, 2001, 1000, -2), (1000, 502));
        assert_eq!(resolve_dimensions(4001, 2000, -2, 500), (1002, 500));
    }
}
