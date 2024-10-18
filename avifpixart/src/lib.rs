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

#![allow(clippy::missing_safety_doc)]

use pic_scale::{
    ImageSize, ImageStore, ResamplingFunction, Scaler, Scaling, ScalingU16, ThreadingPolicy,
};
use std::slice;
use yuvutils_rs::{
    yuv400_p16_to_rgba16, yuv400_p16_with_alpha_to_rgba16, yuv400_to_rgba,
    yuv400_with_alpha_to_rgba, yuv420_p16_to_rgba16, yuv420_p16_with_alpha_to_rgba16,
    yuv420_to_rgba, yuv420_with_alpha_to_rgba, yuv422_p16_to_rgba16,
    yuv422_p16_with_alpha_to_rgba16, yuv422_to_rgba, yuv422_with_alpha_to_rgba,
    yuv444_p16_to_rgba16, yuv444_p16_with_alpha_to_rgba16, yuv444_to_rgba,
    yuv444_with_alpha_to_rgba, YuvBytesPacking, YuvEndianness, YuvStandardMatrix,
};

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Ord, PartialOrd, Eq)]
pub enum YuvRange {
    Tv,
    Pc,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Ord, PartialOrd, Eq)]
pub enum YuvMatrix {
    Bt601,
    Bt709,
    Bt2020,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Ord, PartialOrd, Eq)]
pub enum YuvType {
    Yuv420,
    Yuv422,
    Yuv444,
}

#[no_mangle]
pub unsafe extern "C-unwind" fn weave_yuv8_to_rgba8(
    y_plane: *const u8,
    y_stride: u32,
    u_plane: *const u8,
    u_stride: u32,
    v_plane: *const u8,
    v_stride: u32,
    rgba: *mut u8,
    rgba_stride: u32,
    width: u32,
    height: u32,
    range: YuvRange,
    yuv_matrix: YuvMatrix,
    yuv_type: YuvType,
) {
    let y_slice = slice::from_raw_parts(y_plane, y_stride as usize * height as usize);
    let u_slice = if yuv_type == YuvType::Yuv420 {
        slice::from_raw_parts(u_plane, u_stride as usize * ((height + 1) / 2) as usize)
    } else {
        slice::from_raw_parts(u_plane, u_stride as usize * height as usize)
    };
    let v_slice = if yuv_type == YuvType::Yuv420 {
        slice::from_raw_parts(v_plane, v_stride as usize * ((height + 1) / 2) as usize)
    } else {
        slice::from_raw_parts(v_plane, v_stride as usize * height as usize)
    };
    let rgba_slice = slice::from_raw_parts_mut(rgba, height as usize * rgba_stride as usize);

    let yuv_range = match range {
        YuvRange::Tv => yuvutils_rs::YuvRange::TV,
        YuvRange::Pc => yuvutils_rs::YuvRange::Full,
    };

    let matrix = match yuv_matrix {
        YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
        YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
        YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
    };

    let callee = match yuv_type {
        YuvType::Yuv420 => yuv420_to_rgba,
        YuvType::Yuv422 => yuv422_to_rgba,
        YuvType::Yuv444 => yuv444_to_rgba,
    };

    callee(
        y_slice,
        y_stride,
        u_slice,
        u_stride,
        v_slice,
        v_stride,
        rgba_slice,
        rgba_stride,
        width,
        height,
        yuv_range,
        matrix,
    );
}

#[no_mangle]
pub unsafe extern "C-unwind" fn weave_yuv400_to_rgba8(
    y_plane: *const u8,
    y_stride: u32,
    rgba: *mut u8,
    rgba_stride: u32,
    width: u32,
    height: u32,
    range: YuvRange,
    yuv_matrix: YuvMatrix,
) {
    let y_slice = slice::from_raw_parts(y_plane, y_stride as usize * height as usize);
    let rgba_slice = slice::from_raw_parts_mut(rgba, height as usize * rgba_stride as usize);

    let yuv_range = match range {
        YuvRange::Tv => yuvutils_rs::YuvRange::TV,
        YuvRange::Pc => yuvutils_rs::YuvRange::Full,
    };

    let matrix = match yuv_matrix {
        YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
        YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
        YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
    };

    yuv400_to_rgba(
        y_slice,
        y_stride,
        rgba_slice,
        rgba_stride,
        width,
        height,
        yuv_range,
        matrix,
    );
}

#[no_mangle]
pub unsafe extern "C-unwind" fn weave_yuv400_with_alpha_to_rgba8(
    y_plane: *const u8,
    y_stride: u32,
    a_plane: *const u8,
    a_stride: u32,
    rgba: *mut u8,
    rgba_stride: u32,
    width: u32,
    height: u32,
    range: YuvRange,
    yuv_matrix: YuvMatrix,
) {
    let y_slice = slice::from_raw_parts(y_plane, y_stride as usize * height as usize);
    let a_plane = slice::from_raw_parts(a_plane, a_plane as usize * height as usize);
    let rgba_slice = slice::from_raw_parts_mut(rgba, height as usize * rgba_stride as usize);

    let yuv_range = match range {
        YuvRange::Tv => yuvutils_rs::YuvRange::TV,
        YuvRange::Pc => yuvutils_rs::YuvRange::Full,
    };

    let matrix = match yuv_matrix {
        YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
        YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
        YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
    };

    yuv400_with_alpha_to_rgba(
        y_slice,
        y_stride,
        a_plane,
        a_stride,
        rgba_slice,
        rgba_stride,
        width,
        height,
        yuv_range,
        matrix,
    );
}

#[no_mangle]
pub unsafe extern "C-unwind" fn weave_yuv400_p16_to_rgba16(
    y_plane: *const u16,
    y_stride: u32,
    rgba: *mut u16,
    rgba_stride: u32,
    bit_depth: u32,
    width: u32,
    height: u32,
    range: YuvRange,
    yuv_matrix: YuvMatrix,
) {
    let y_slice = slice::from_raw_parts(y_plane, y_stride as usize);
    let rgba_slice = slice::from_raw_parts_mut(rgba, width as usize * 4);

    let yuv_range = match range {
        YuvRange::Tv => yuvutils_rs::YuvRange::TV,
        YuvRange::Pc => yuvutils_rs::YuvRange::Full,
    };

    let matrix = match yuv_matrix {
        YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
        YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
        YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
    };

    yuv400_p16_to_rgba16(
        y_slice,
        y_stride,
        rgba_slice,
        rgba_stride,
        bit_depth,
        width,
        height,
        yuv_range,
        matrix,
        YuvEndianness::LittleEndian,
        YuvBytesPacking::LeastSignificantBytes,
    );
}

#[no_mangle]
pub unsafe extern "C-unwind" fn weave_yuv400_p16_with_alpha_to_rgba16(
    y_plane: *const u16,
    y_stride: u32,
    a_plane: *const u16,
    a_stride: u32,
    rgba: *mut u16,
    rgba_stride: u32,
    bit_depth: u32,
    width: u32,
    height: u32,
    range: YuvRange,
    yuv_matrix: YuvMatrix,
) {
    let y_slice = slice::from_raw_parts(y_plane, y_stride as usize);
    let rgba_slice = slice::from_raw_parts_mut(rgba, width as usize * 4);

    let yuv_range = match range {
        YuvRange::Tv => yuvutils_rs::YuvRange::TV,
        YuvRange::Pc => yuvutils_rs::YuvRange::Full,
    };

    let matrix = match yuv_matrix {
        YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
        YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
        YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
    };

    let a_slice = slice::from_raw_parts(a_plane, width as usize * height as usize);

    yuv400_p16_with_alpha_to_rgba16(
        y_slice,
        y_stride,
        a_slice,
        a_stride,
        rgba_slice,
        rgba_stride,
        bit_depth,
        width,
        height,
        yuv_range,
        matrix,
        YuvEndianness::LittleEndian,
        YuvBytesPacking::LeastSignificantBytes,
    );
}

#[no_mangle]
pub unsafe extern "C-unwind" fn weave_yuv8_with_alpha_to_rgba8(
    y_plane: *const u8,
    y_stride: u32,
    u_plane: *const u8,
    u_stride: u32,
    v_plane: *const u8,
    v_stride: u32,
    a_plane: *const u8,
    a_stride: u32,
    rgba: *mut u8,
    rgba_stride: u32,
    width: u32,
    height: u32,
    range: YuvRange,
    yuv_matrix: YuvMatrix,
    yuv_type: YuvType,
) {
    let y_slice = slice::from_raw_parts(y_plane, y_stride as usize * height as usize);
    let a_slice = slice::from_raw_parts(a_plane, a_stride as usize * height as usize);
    let u_slice = if yuv_type == YuvType::Yuv420 {
        slice::from_raw_parts(u_plane, u_stride as usize * ((height + 1) / 2) as usize)
    } else {
        slice::from_raw_parts(u_plane, u_stride as usize * height as usize)
    };
    let v_slice = if yuv_type == YuvType::Yuv420 {
        slice::from_raw_parts(v_plane, v_stride as usize * ((height + 1) / 2) as usize)
    } else {
        slice::from_raw_parts(v_plane, v_stride as usize * height as usize)
    };
    let rgba_slice = slice::from_raw_parts_mut(rgba, height as usize * rgba_stride as usize);

    let yuv_range = match range {
        YuvRange::Tv => yuvutils_rs::YuvRange::TV,
        YuvRange::Pc => yuvutils_rs::YuvRange::Full,
    };

    let matrix = match yuv_matrix {
        YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
        YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
        YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
    };

    let callee = match yuv_type {
        YuvType::Yuv420 => yuv420_with_alpha_to_rgba,
        YuvType::Yuv422 => yuv422_with_alpha_to_rgba,
        YuvType::Yuv444 => yuv444_with_alpha_to_rgba,
    };

    callee(
        y_slice,
        y_stride,
        u_slice,
        u_stride,
        v_slice,
        v_stride,
        a_slice,
        a_stride,
        rgba_slice,
        rgba_stride,
        width,
        height,
        yuv_range,
        matrix,
        false,
    );
}

#[no_mangle]
pub unsafe extern "C-unwind" fn weave_yuv16_to_rgba16(
    y_plane: *const u16,
    y_stride: u32,
    u_plane: *const u16,
    u_stride: u32,
    v_plane: *const u16,
    v_stride: u32,
    rgba: *mut u16,
    rgba_stride: u32,
    bit_depth: u32,
    width: u32,
    height: u32,
    range: YuvRange,
    yuv_matrix: YuvMatrix,
    yuv_type: YuvType,
) {
    let y_slice = slice::from_raw_parts(y_plane, width as usize * height as usize);
    let u_slice = if yuv_type == YuvType::Yuv420 {
        slice::from_raw_parts(
            u_plane,
            (width as usize + 1) / 2 * ((height + 1) / 2) as usize,
        )
    } else if yuv_type == YuvType::Yuv422 {
        slice::from_raw_parts(u_plane, width as usize * ((height + 1) / 2) as usize)
    } else {
        slice::from_raw_parts(u_plane, width as usize * height as usize)
    };
    let v_slice = if yuv_type == YuvType::Yuv420 {
        slice::from_raw_parts(
            v_plane,
            (width as usize + 1) / 2 * ((height + 1) / 2) as usize,
        )
    } else if yuv_type == YuvType::Yuv422 {
        slice::from_raw_parts(v_plane, width as usize * ((height + 1) / 2) as usize)
    } else {
        slice::from_raw_parts(v_plane, width as usize * height as usize)
    };
    let rgba_slice = slice::from_raw_parts_mut(rgba, height as usize * width as usize * 4);

    let yuv_range = match range {
        YuvRange::Tv => yuvutils_rs::YuvRange::TV,
        YuvRange::Pc => yuvutils_rs::YuvRange::Full,
    };

    let matrix = match yuv_matrix {
        YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
        YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
        YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
    };

    let callee = match yuv_type {
        YuvType::Yuv420 => yuv420_p16_to_rgba16,
        YuvType::Yuv422 => yuv422_p16_to_rgba16,
        YuvType::Yuv444 => yuv444_p16_to_rgba16,
    };

    callee(
        y_slice,
        y_stride,
        u_slice,
        u_stride,
        v_slice,
        v_stride,
        rgba_slice,
        rgba_stride,
        bit_depth as usize,
        width,
        height,
        yuv_range,
        matrix,
        YuvEndianness::LittleEndian,
        YuvBytesPacking::LeastSignificantBytes,
    );
}

#[no_mangle]
pub unsafe extern "C-unwind" fn weave_yuv16_with_alpha_to_rgba16(
    y_plane: *const u16,
    y_stride: u32,
    u_plane: *const u16,
    u_stride: u32,
    v_plane: *const u16,
    v_stride: u32,
    a_plane: *const u16,
    a_stride: u32,
    rgba: *mut u16,
    rgba_stride: u32,
    bit_depth: u32,
    width: u32,
    height: u32,
    range: YuvRange,
    yuv_matrix: YuvMatrix,
    yuv_type: YuvType,
) {
    let y_slice = slice::from_raw_parts(y_plane, width as usize * height as usize);
    let a_slice = slice::from_raw_parts(a_plane, width as usize * height as usize);
    let u_slice = if yuv_type == YuvType::Yuv420 {
        slice::from_raw_parts(
            u_plane,
            (width as usize + 1) / 2 * ((height + 1) / 2) as usize,
        )
    } else if yuv_type == YuvType::Yuv422 {
        slice::from_raw_parts(u_plane, width as usize * ((height + 1) / 2) as usize)
    } else {
        slice::from_raw_parts(u_plane, width as usize * height as usize)
    };
    let v_slice = if yuv_type == YuvType::Yuv420 {
        slice::from_raw_parts(
            v_plane,
            (width as usize + 1) / 2 * ((height + 1) / 2) as usize,
        )
    } else if yuv_type == YuvType::Yuv422 {
        slice::from_raw_parts(v_plane, width as usize * ((height + 1) / 2) as usize)
    } else {
        slice::from_raw_parts(v_plane, width as usize * height as usize)
    };
    let rgba_slice = slice::from_raw_parts_mut(rgba, height as usize * width as usize * 4);

    let yuv_range = match range {
        YuvRange::Tv => yuvutils_rs::YuvRange::TV,
        YuvRange::Pc => yuvutils_rs::YuvRange::Full,
    };

    let matrix = match yuv_matrix {
        YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
        YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
        YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
    };

    let callee = match yuv_type {
        YuvType::Yuv420 => yuv420_p16_with_alpha_to_rgba16,
        YuvType::Yuv422 => yuv422_p16_with_alpha_to_rgba16,
        YuvType::Yuv444 => yuv444_p16_with_alpha_to_rgba16,
    };

    callee(
        y_slice,
        y_stride,
        u_slice,
        u_stride,
        v_slice,
        v_stride,
        a_slice,
        a_stride,
        rgba_slice,
        rgba_stride,
        bit_depth as usize,
        width,
        height,
        yuv_range,
        matrix,
        YuvEndianness::LittleEndian,
        YuvBytesPacking::LeastSignificantBytes,
    );
}

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
    method: u32,
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

    let mut scaler = Scaler::new(if method == 3 {
        ResamplingFunction::Lanczos3
    } else if method == 1 {
        ResamplingFunction::Nearest
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

    for (dst_chunk, src_chunk) in dst_slice
        .chunks_mut(dst_stride as usize)
        .zip(new_store.as_bytes().chunks(new_width as usize * 4))
    {
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
    method: u32,
) {
    let mut _src_slice = vec![0u16; width as usize * height as usize * 4];

    for y in 0..height as usize {
        let lane = (src as *const u8).add(y * src_stride) as *const u16;
        let dst_lane = _src_slice.get_unchecked_mut((y * (width as usize * 4))..);
        for x in 0..width as usize {
            let px = lane.add(x * 4);
            let dst_px = dst_lane.get_unchecked_mut((x * 4)..);
            *dst_px.get_unchecked_mut(0) = px.read_unaligned();
            *dst_px.get_unchecked_mut(1) = px.add(1).read_unaligned();
            *dst_px.get_unchecked_mut(2) = px.add(2).read_unaligned();
            *dst_px.get_unchecked_mut(3) = px.add(3).read_unaligned();
        }
    }

    let _source_store =
        ImageStore::<u16, 4>::new(_src_slice, width as usize, height as usize).unwrap();

    let mut scaler = Scaler::new(if method == 3 {
        ResamplingFunction::Lanczos3
    } else if method == 1 {
        ResamplingFunction::Nearest
    } else {
        ResamplingFunction::Bilinear
    });
    scaler.set_threading_policy(ThreadingPolicy::Adaptive);

    let _new_store = scaler.resize_rgba_u16(
        ImageSize::new(new_width as usize, new_height as usize),
        _source_store,
        bit_depth,
        false,
    );
    let dst_slice = unsafe {
        slice::from_raw_parts_mut(
            dst as *mut u16,
            new_width as usize * 4 * new_height as usize,
        )
    };
    for (src, dst) in _new_store.as_bytes().iter().zip(dst_slice.iter_mut()) {
        *dst = *src;
    }
}
