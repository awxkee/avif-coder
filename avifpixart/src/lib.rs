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

use std::slice;
use pic_scale::{ImageSize, ImageStore, ResamplingFunction, Scaler, Scaling, ScalingU16, ThreadingPolicy};

#[no_mangle]
pub unsafe extern "C-unwind" fn weave_scale_u8(
    src: *const u8,
    src_stride: u32,
    width: u32,
    height: u32,
    dst: *const u8,
    dst_stride: u32,
    new_width: u32,
    new_height: u32,
    high_precision: bool,
) {
    let mut src_slice = vec![0u8; width as usize * height as usize * 4];

    for y in 0..height as usize {
        let lane = src.add(y * src_stride as usize);
        let dst_lane = src_slice.get_unchecked_mut((y * (width as usize * 4))..);
        for x in 0..width as usize {
            let px = lane.add(x * 4);
            let dst_px = dst_lane.get_unchecked_mut((x * 4)..);
            unsafe {
                *dst_px.get_unchecked_mut(0) = px.read_unaligned();
                *dst_px.get_unchecked_mut(1) = px.add(1).read_unaligned();
                *dst_px.get_unchecked_mut(2) = px.add(2).read_unaligned();
                *dst_px.get_unchecked_mut(3) = px.add(3).read_unaligned();
            }
        }
    }

    let source_store =
        ImageStore::<u8, 4>::new(src_slice, width as usize, height as usize).unwrap();

    let mut scaler = Scaler::new(if high_precision {
        ResamplingFunction::Lanczos3
    } else {
        ResamplingFunction::Bilinear
    });

    scaler.set_threading_policy(ThreadingPolicy::Adaptive);
    let dst_slice = unsafe {
        slice::from_raw_parts_mut(dst as *mut u8, dst_stride as usize * new_height as usize)
    };

    let new_store = scaler.resize_rgba(
        ImageSize::new(new_width as usize, new_height as usize),
        source_store,
        false,
    );

    for (dst_chunk, src_chunk) in dst_slice.chunks_mut(dst_stride as usize).zip(new_store.as_bytes().chunks(new_width as usize * 4)) {
        for (dst, src) in dst_chunk.iter_mut().zip(src_chunk.iter()) {
            *dst = *src;
        }
    }
}

#[no_mangle]
pub unsafe extern "C-unwind" fn weave_scale_u16(
    src: *const u16,
    src_stride: usize,
    width: u32,
    height: u32,
    dst: *const u16,
    new_width: u32,
    new_height: u32,
    bit_depth: usize,
    high_precision: bool,
) {
    let mut _src_slice = vec![0u16; width as usize * height as usize * 4];

    for y in 0..height as usize {
        let lane = (src as *const u8).add(y * src_stride) as *const u16;
        let dst_lane = _src_slice.get_unchecked_mut((y * (width as usize * 4))..);
        for x in 0..width as usize {
            let px = lane.add(x * 4);
            let dst_px = dst_lane.get_unchecked_mut((x * 4)..);
            unsafe {
                *dst_px.get_unchecked_mut(0) = px.read_unaligned();
                *dst_px.get_unchecked_mut(1) = px.add(1).read_unaligned();
                *dst_px.get_unchecked_mut(2) = px.add(2).read_unaligned();
                *dst_px.get_unchecked_mut(3) = px.add(3).read_unaligned();
            }
        }
    }

    let _source_store = ImageStore::<u16, 4>::new(
        _src_slice,
        width as usize,
        height as usize,
    )
        .unwrap();

    let mut scaler = Scaler::new(if high_precision {
        ResamplingFunction::Lanczos3
    } else {
        ResamplingFunction::Bilinear
    });
    scaler.set_threading_policy(ThreadingPolicy::Adaptive);

    let _new_store_stride = new_width * 4 * std::mem::size_of::<u16>() as u32;

    let _new_store = scaler.resize_rgba_u16(
        ImageSize::new(new_width as usize, new_height as usize),
        _source_store,
        bit_depth,
        false,
    );
    let dst_slice = unsafe {
        slice::from_raw_parts_mut(dst as *mut u16, new_width as usize * 4 * new_height as usize)
    };
    for (src, dst) in _new_store.as_bytes().iter().zip(dst_slice.iter_mut()) {
        *dst = *src;
    }
}
