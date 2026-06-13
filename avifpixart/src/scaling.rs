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

fn resolve_dimensions(
    src_width: u32,
    src_height: u32,
    new_width: i32,
    new_height: i32,
) -> (usize, usize) {
    match (new_width, new_height) {
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
            (w as usize, (h.max(1) + 1) & !1)
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
            ((w.max(1) + 1) & !1, h as usize)
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

impl ScalingFunction {
    pub(crate) fn to_ps(self) -> ResamplingFunction {
        match self {
            ScalingFunction::Lanczos3 => ResamplingFunction::Lanczos3,
            ScalingFunction::Nearest => ResamplingFunction::Nearest,
            ScalingFunction::Bilinear => ResamplingFunction::Bilinear,
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
    let source_store = ImageStore::<u8, 4> {
        buffer: src,
        channels: 4,
        width: width as usize,
        height: height as usize,
        stride: src_stride as usize,
        bit_depth: 8,
    };

    let scaler = Scaler::new(ResamplingFunction::MitchellNetravalli)
        .set_threading_policy(ThreadingPolicy::Single);

    let (new_width, new_height) = resolve_dimensions(width, height, new_width, new_height);

    let (scale_w, scale_h, crop_x, crop_y, crop_w, crop_h) = match scale_mode {
        WeaveScaleMode::ScaleToFill => {
            // ScaleToFill: scale up to cover, then center crop
            let x_factor = new_width as f64 / width as f64;
            let y_factor = new_height as f64 / height as f64;
            let scale = x_factor.max(y_factor);
            let sw = ((width as f64 * scale).round() as usize).max(1);
            let sh = ((height as f64 * scale).round() as usize).max(1);
            let cx = ((sw as i64 - new_width as i64) / 2).max(0) as usize;
            let cy = ((sh as i64 - new_height as i64) / 2).max(0) as usize;
            // guard: crop window can't exceed scaled size due to rounding
            let cw = new_width.min(sw);
            let ch = new_height.min(sh);
            (sw, sh, cx, cy, cw, ch)
        }
        WeaveScaleMode::ScaleToFit => {
            // ScaleToFit: scale to fit within bounds, crop excess (will be 0 on one axis)
            let x_factor = new_width as f64 / width as f64;
            let y_factor = new_height as f64 / height as f64;
            let scale = x_factor.min(y_factor);
            let sw = ((width as f64 * scale).round() as usize).max(1);
            let sh = ((height as f64 * scale).round() as usize).max(1);
            let cx = ((sw as i64 - new_width as i64) / 2).max(0) as usize;
            let cy = ((sh as i64 - new_height as i64) / 2).max(0) as usize;
            let cw = sw.min(new_width);
            let ch = sh.min(new_height);
            (sw, sh, cx, cy, cw, ch)
        }
        WeaveScaleMode::JustResize => {
            // JustResize
            (new_width, new_height, 0, 0, new_width, new_height)
        }
    };

    let mut scaled_store = ImageStoreMut::try_alloc(scale_w, scale_h)?;

    let plan =
        scaler.plan_rgba_resampling(source_store.size(), scaled_store.size(), premultiply_alpha)?;
    plan.resample(&source_store, &mut scaled_store)?;

    let final_store = if crop_x > 0 || crop_y > 0 || crop_w != scale_w || crop_h != scale_h {
        scaled_store.crop_with_copy(crop_x, crop_y, crop_w, crop_h)?
    } else {
        scaled_store
    };
    Ok(final_store)
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
            for (dst, src) in dst.iter_mut().zip(src.chunks_exact(2)) {
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
    let source_store = ImageStore::<u16, 4> {
        buffer: src,
        channels: 4,
        width: width as usize,
        height: height as usize,
        stride: src_stride,
        bit_depth,
    };

    let scaler = Scaler::new(ResamplingFunction::MitchellNetravalli)
        .set_threading_policy(ThreadingPolicy::Single)
        .set_workload_strategy(WorkloadStrategy::PreferQuality);

    let (new_width, new_height) = resolve_dimensions(width, height, new_width, new_height);

    let (scale_w, scale_h, crop_x, crop_y, crop_w, crop_h) = match scale_mode {
        WeaveScaleMode::ScaleToFill => {
            // ScaleToFill: scale up to cover, then center crop
            let x_factor = new_width as f64 / width as f64;
            let y_factor = new_height as f64 / height as f64;
            let scale = x_factor.max(y_factor);
            let sw = ((width as f64 * scale).round() as usize).max(1);
            let sh = ((height as f64 * scale).round() as usize).max(1);
            let cx = ((sw as i64 - new_width as i64) / 2).max(0) as usize;
            let cy = ((sh as i64 - new_height as i64) / 2).max(0) as usize;
            // guard: crop window can't exceed scaled size due to rounding
            let cw = new_width.min(sw);
            let ch = new_height.min(sh);
            (sw, sh, cx, cy, cw, ch)
        }
        WeaveScaleMode::ScaleToFit => {
            // ScaleToFit: scale to fit within bounds, crop excess (will be 0 on one axis)
            let x_factor = new_width as f64 / width as f64;
            let y_factor = new_height as f64 / height as f64;
            let scale = x_factor.min(y_factor);
            let sw = ((width as f64 * scale).round() as usize).max(1);
            let sh = ((height as f64 * scale).round() as usize).max(1);
            let cx = ((sw as i64 - new_width as i64) / 2).max(0) as usize;
            let cy = ((sh as i64 - new_height as i64) / 2).max(0) as usize;
            let cw = sw.min(new_width);
            let ch = sh.min(new_height);
            (sw, sh, cx, cy, cw, ch)
        }
        WeaveScaleMode::JustResize => {
            // JustResize
            (new_width, new_height, 0, 0, new_width, new_height)
        }
    };

    let Ok(mut scaled_store) = ImageStoreMut::try_alloc_with_depth(scale_w, scale_h, bit_depth)
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

    let final_store = if crop_x > 0 || crop_y > 0 || crop_w != scale_w || crop_h != scale_h {
        scaled_store.crop_with_copy(crop_x, crop_y, crop_w, crop_h)?
    } else {
        scaled_store
    };
    Ok(final_store)
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
                for (dst, src) in dst.iter_mut().zip(src.chunks_exact(2)) {
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
                for (src, dst) in src.iter().zip(dst.chunks_exact_mut(2)) {
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
