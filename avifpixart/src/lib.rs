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
#![feature(f16)]

mod cvt;
mod rgb_to_yuv;
mod support;

use crate::support::{transmute_const_ptr16, SliceStoreMut};
use pic_scale::{
    BufferStore, ImageStore, ImageStoreMut, ResamplingFunction, Scaler, Scaling, ScalingU16,
    ThreadingPolicy, WorkloadStrategy,
};
use std::fmt::Debug;
use std::slice;
use yuvutils_rs::{
    convert_rgba16_to_f16, gb10_alpha_to_rgba10, gb10_to_rgba10, gb10_to_rgba_f16,
    gb12_alpha_to_rgba12, gb12_to_rgba12, gb12_to_rgba_f16, gbr_to_rgba, gbr_with_alpha_to_rgba,
    i010_alpha_to_rgba10, i010_to_rgba10, i010_to_rgba_f16, i012_alpha_to_rgba12, i012_to_rgba12,
    i012_to_rgba_f16, i210_alpha_to_rgba10, i210_to_rgba10, i210_to_rgba_f16, i212_alpha_to_rgba12,
    i212_to_rgba12, i212_to_rgba_f16, i410_alpha_to_rgba10, i410_to_rgba10, i410_to_rgba_f16,
    i412_alpha_to_rgba12, i412_to_rgba12, i412_to_rgba_f16, icgc010_alpha_to_rgba10,
    icgc010_to_rgba10, icgc012_alpha_to_rgba12, icgc012_to_rgba12, icgc210_alpha_to_rgba10,
    icgc210_to_rgba10, icgc212_alpha_to_rgba12, icgc212_to_rgba12, icgc410_alpha_to_rgba10,
    icgc410_to_rgba10, icgc412_alpha_to_rgba12, icgc412_to_rgba12, rgba_to_gbr, rgba_to_ycgco420,
    rgba_to_ycgco422, rgba_to_ycgco444, rgba_to_yuv400, rgba_to_yuv420, rgba_to_yuv422,
    rgba_to_yuv444, y010_alpha_to_rgba10, y010_to_rgba10, y012_alpha_to_rgba12, y012_to_rgba12,
    ycgco420_alpha_to_rgba, ycgco420_to_rgba, ycgco422_alpha_to_rgba, ycgco422_to_rgba,
    ycgco444_alpha_to_rgba, ycgco444_to_rgba, yuv400_alpha_to_rgba, yuv400_to_rgba,
    yuv420_alpha_to_rgba, yuv420_to_rgba, yuv422_alpha_to_rgba, yuv422_to_rgba,
    yuv444_alpha_to_rgba, yuv444_to_rgba, BufferStoreMut, YuvConversionMode, YuvGrayAlphaImage,
    YuvGrayImage, YuvGrayImageMut, YuvPlanarImage, YuvPlanarImageMut, YuvPlanarImageWithAlpha,
    YuvStandardMatrix,
};

pub use cvt::{
    weave_cvt_rgba16_to_ar30, weave_cvt_rgba16_to_rgba_f16, weave_cvt_rgba8_to_ar30,
    weave_cvt_rgba8_to_rgba_f16, weave_premultiply_rgba_f16,
};
pub use rgb_to_yuv::{weave_rgba8_to_y08, weave_rgba8_to_yuv8};

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
    Identity,
    YCgCo,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Ord, PartialOrd, Eq)]
pub enum YuvType {
    Yuv420,
    Yuv422,
    Yuv444,
}

#[no_mangle]
pub extern "C" fn weave_yuv8_to_rgba8(
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
    unsafe {
        let y_slice = slice::from_raw_parts(y_plane, y_stride as usize * height as usize);
        let u_slice = if yuv_type == YuvType::Yuv420 {
            slice::from_raw_parts(u_plane, u_stride as usize * (height as usize).div_ceil(2))
        } else {
            slice::from_raw_parts(u_plane, u_stride as usize * height as usize)
        };
        let v_slice = if yuv_type == YuvType::Yuv420 {
            slice::from_raw_parts(v_plane, v_stride as usize * (height as usize).div_ceil(2))
        } else {
            slice::from_raw_parts(v_plane, v_stride as usize * height as usize)
        };
        let rgba_slice = slice::from_raw_parts_mut(rgba, height as usize * rgba_stride as usize);

        let yuv_range = match range {
            YuvRange::Tv => yuvutils_rs::YuvRange::Limited,
            YuvRange::Pc => yuvutils_rs::YuvRange::Full,
        };

        let planar_image = YuvPlanarImage {
            y_plane: y_slice,
            y_stride,
            u_plane: u_slice,
            u_stride,
            v_plane: v_slice,
            v_stride,
            width,
            height,
        };

        if yuv_matrix == YuvMatrix::Identity {
            assert_eq!(yuv_type, YuvType::Yuv444, "Identity exists only on 4:4:4");
            gbr_to_rgba(&planar_image, rgba_slice, rgba_stride, yuv_range).unwrap();
        } else if yuv_matrix == YuvMatrix::YCgCo {
            let callee = match yuv_type {
                YuvType::Yuv420 => ycgco420_to_rgba,
                YuvType::Yuv422 => ycgco422_to_rgba,
                YuvType::Yuv444 => ycgco444_to_rgba,
            };
            callee(&planar_image, rgba_slice, rgba_stride, yuv_range).unwrap();
        } else {
            let matrix = match yuv_matrix {
                YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
                YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
                YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
                YuvMatrix::Identity => unreachable!(),
                YuvMatrix::YCgCo => unreachable!(),
            };

            let callee = match yuv_type {
                YuvType::Yuv420 => yuv420_to_rgba,
                YuvType::Yuv422 => yuv422_to_rgba,
                YuvType::Yuv444 => yuv444_to_rgba,
            };

            callee(&planar_image, rgba_slice, rgba_stride, yuv_range, matrix).unwrap();
        }
    }
}

#[no_mangle]
pub extern "C" fn weave_yuv400_to_rgba8(
    y_plane: *const u8,
    y_stride: u32,
    rgba: *mut u8,
    rgba_stride: u32,
    width: u32,
    height: u32,
    range: YuvRange,
    yuv_matrix: YuvMatrix,
) {
    unsafe {
        let y_slice = slice::from_raw_parts(y_plane, y_stride as usize * height as usize);
        let rgba_slice = slice::from_raw_parts_mut(rgba, height as usize * rgba_stride as usize);

        let yuv_range = match range {
            YuvRange::Tv => yuvutils_rs::YuvRange::Limited,
            YuvRange::Pc => yuvutils_rs::YuvRange::Full,
        };

        let matrix = match yuv_matrix {
            YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
            YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
            YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
            YuvMatrix::Identity => unreachable!("Identity exists only on 4:4:4"),
            YuvMatrix::YCgCo => unreachable!(),
        };

        let gray_image = YuvGrayImage {
            y_plane: y_slice,
            y_stride,
            width,
            height,
        };

        yuv400_to_rgba(&gray_image, rgba_slice, rgba_stride, yuv_range, matrix).unwrap();
    }
}

#[no_mangle]
pub extern "C" fn weave_yuv400_with_alpha_to_rgba8(
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
    unsafe {
        let y_slice = slice::from_raw_parts(y_plane, y_stride as usize * height as usize);
        let a_plane = slice::from_raw_parts(a_plane, a_plane as usize * height as usize);
        let rgba_slice = slice::from_raw_parts_mut(rgba, height as usize * rgba_stride as usize);

        let yuv_range = match range {
            YuvRange::Tv => yuvutils_rs::YuvRange::Limited,
            YuvRange::Pc => yuvutils_rs::YuvRange::Full,
        };

        let gray_with_alpha = YuvGrayAlphaImage {
            y_plane: y_slice,
            y_stride,
            a_plane,
            a_stride,
            width,
            height,
        };

        let matrix = match yuv_matrix {
            YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
            YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
            YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
            YuvMatrix::Identity => unreachable!("Identity exists only on 4:4:4"),
            YuvMatrix::YCgCo => unreachable!(),
        };

        yuv400_alpha_to_rgba(&gray_with_alpha, rgba_slice, rgba_stride, yuv_range, matrix).unwrap();
    }
}

#[no_mangle]
pub extern "C" fn weave_yuv400_p16_to_rgba16(
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
    unsafe {
        let y_slice = transmute_const_ptr16(
            y_plane,
            y_stride as usize,
            width as usize,
            height as usize,
            1,
        );
        let rgba_slice8 =
            slice::from_raw_parts_mut(rgba as *mut u8, height as usize * rgba_stride as usize);
        let mut is_owned_rgba = false;
        let rgba_stride_u16;

        let mut working_slice = if let Ok(casted) = bytemuck::try_cast_slice_mut(rgba_slice8) {
            rgba_stride_u16 = rgba_stride / 2;
            SliceStoreMut::Borrowed(casted)
        } else {
            is_owned_rgba = true;
            rgba_stride_u16 = width * 4;
            SliceStoreMut::Owned(vec![0u16; width as usize * 4 * height as usize])
        };

        let yuv_range = match range {
            YuvRange::Tv => yuvutils_rs::YuvRange::Limited,
            YuvRange::Pc => yuvutils_rs::YuvRange::Full,
        };

        let matrix = match yuv_matrix {
            YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
            YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
            YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
            YuvMatrix::Identity => unreachable!("Identity exists only on 4:4:4"),
            YuvMatrix::YCgCo => unreachable!(),
        };

        let gray_image = YuvGrayImage {
            y_plane: y_slice.0.as_ref(),
            y_stride: y_slice.1 as u32,
            width,
            height,
        };

        let dispatch = if bit_depth == 10 {
            y010_to_rgba10
        } else {
            y012_to_rgba12
        };

        dispatch(
            &gray_image,
            working_slice.borrow_mut(),
            rgba_stride_u16,
            yuv_range,
            matrix,
        )
        .unwrap();

        let target =
            slice::from_raw_parts_mut(rgba as *mut u8, height as usize * rgba_stride as usize);
        if is_owned_rgba {
            for (dst, src) in target.chunks_exact_mut(rgba_stride as usize).zip(
                working_slice
                    .borrow()
                    .chunks_exact(rgba_stride_u16 as usize),
            ) {
                for (dst, src) in dst.chunks_exact_mut(2).zip(src.iter()) {
                    let bytes = src.to_ne_bytes();
                    dst[0] = bytes[0];
                    dst[1] = bytes[1];
                }
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn weave_yuv400_p16_with_alpha_to_rgba16(
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
    unsafe {
        let y_slice = transmute_const_ptr16(
            y_plane,
            y_stride as usize,
            width as usize,
            height as usize,
            1,
        );

        let yuv_range = match range {
            YuvRange::Tv => yuvutils_rs::YuvRange::Limited,
            YuvRange::Pc => yuvutils_rs::YuvRange::Full,
        };

        let matrix = match yuv_matrix {
            YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
            YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
            YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
            YuvMatrix::Identity => unreachable!("Identity exists only on 4:4:4"),
            YuvMatrix::YCgCo => unreachable!(),
        };

        let a_orig = transmute_const_ptr16(
            a_plane,
            a_stride as usize,
            width as usize,
            height as usize,
            1,
        );

        let rgba_slice8 =
            slice::from_raw_parts_mut(rgba as *mut u8, height as usize * rgba_stride as usize);
        let mut is_owned_rgba = false;
        let rgba_stride_u16;

        let mut working_slice = if let Ok(casted) = bytemuck::try_cast_slice_mut(rgba_slice8) {
            rgba_stride_u16 = rgba_stride / 2;
            SliceStoreMut::Borrowed(casted)
        } else {
            is_owned_rgba = true;
            rgba_stride_u16 = width * 4;
            SliceStoreMut::Owned(vec![0u16; width as usize * 4 * height as usize])
        };

        let gray_alpha = YuvGrayAlphaImage {
            y_plane: y_slice.0.as_ref(),
            y_stride: y_slice.1 as u32,
            a_plane: a_orig.0.as_ref(),
            a_stride: a_orig.1 as u32,
            width,
            height,
        };

        let dispatch = if bit_depth == 10 {
            y010_alpha_to_rgba10
        } else {
            y012_alpha_to_rgba12
        };

        dispatch(
            &gray_alpha,
            working_slice.borrow_mut(),
            rgba_stride_u16,
            yuv_range,
            matrix,
        )
        .unwrap();

        let target =
            slice::from_raw_parts_mut(rgba as *mut u8, height as usize * rgba_stride as usize);
        if is_owned_rgba {
            for (dst, src) in target.chunks_exact_mut(rgba_stride as usize).zip(
                working_slice
                    .borrow()
                    .chunks_exact(rgba_stride_u16 as usize),
            ) {
                for (dst, src) in dst.chunks_exact_mut(2).zip(src.iter()) {
                    let bytes = src.to_ne_bytes();
                    dst[0] = bytes[0];
                    dst[1] = bytes[1];
                }
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn weave_yuv8_with_alpha_to_rgba8(
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
    unsafe {
        let y_slice = slice::from_raw_parts(y_plane, y_stride as usize * height as usize);
        let a_slice = slice::from_raw_parts(a_plane, a_stride as usize * height as usize);
        let u_slice = if yuv_type == YuvType::Yuv420 {
            slice::from_raw_parts(u_plane, u_stride as usize * height.div_ceil(2) as usize)
        } else {
            slice::from_raw_parts(u_plane, u_stride as usize * height as usize)
        };
        let v_slice = if yuv_type == YuvType::Yuv420 {
            slice::from_raw_parts(v_plane, v_stride as usize * height.div_ceil(2) as usize)
        } else {
            slice::from_raw_parts(v_plane, v_stride as usize * height as usize)
        };
        let rgba_slice = slice::from_raw_parts_mut(rgba, height as usize * rgba_stride as usize);

        let yuv_range = match range {
            YuvRange::Tv => yuvutils_rs::YuvRange::Limited,
            YuvRange::Pc => yuvutils_rs::YuvRange::Full,
        };

        let planar_with_alpha = YuvPlanarImageWithAlpha {
            y_plane: y_slice,
            y_stride,
            u_plane: u_slice,
            u_stride,
            v_plane: v_slice,
            v_stride,
            a_plane: a_slice,
            a_stride,
            width,
            height,
        };

        if yuv_matrix == YuvMatrix::Identity {
            assert_eq!(yuv_type, YuvType::Yuv444, "Identity exists only on 4:4:4");
            gbr_with_alpha_to_rgba(&planar_with_alpha, rgba_slice, rgba_stride, yuv_range).unwrap();
        } else if yuv_matrix == YuvMatrix::YCgCo {
            let callee = match yuv_type {
                YuvType::Yuv420 => ycgco420_alpha_to_rgba,
                YuvType::Yuv422 => ycgco422_alpha_to_rgba,
                YuvType::Yuv444 => ycgco444_alpha_to_rgba,
            };
            callee(&planar_with_alpha, rgba_slice, rgba_stride, yuv_range).unwrap();
        } else {
            let matrix = match yuv_matrix {
                YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
                YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
                YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
                YuvMatrix::Identity => unreachable!("Should be handled in another place"),
                YuvMatrix::YCgCo => unreachable!(),
            };

            let callee = match yuv_type {
                YuvType::Yuv420 => yuv420_alpha_to_rgba,
                YuvType::Yuv422 => yuv422_alpha_to_rgba,
                YuvType::Yuv444 => yuv444_alpha_to_rgba,
            };

            callee(
                &planar_with_alpha,
                rgba_slice,
                rgba_stride,
                yuv_range,
                matrix,
                false,
            )
            .unwrap();
        }
    }
}

#[no_mangle]
pub extern "C" fn weave_yuv16_to_rgba16(
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
    unsafe {
        let y_slice = transmute_const_ptr16(
            y_plane,
            y_stride as usize,
            width as usize,
            height as usize,
            1,
        );
        let chroma_height = if yuv_type == YuvType::Yuv420 {
            height.div_ceil(2) as usize
        } else {
            height as usize
        };
        let chroma_width = if yuv_type == YuvType::Yuv420 {
            width.div_ceil(2) as usize
        } else {
            width as usize
        };
        let u_slice =
            transmute_const_ptr16(u_plane, u_stride as usize, chroma_width, chroma_height, 1);
        let v_slice =
            transmute_const_ptr16(v_plane, v_stride as usize, chroma_width, chroma_height, 1);

        let rgba_slice8 =
            slice::from_raw_parts_mut(rgba as *mut u8, height as usize * rgba_stride as usize);
        let mut is_owned_rgba = false;
        let rgba_stride_u16;

        let mut working_slice = if let Ok(casted) = bytemuck::try_cast_slice_mut(rgba_slice8) {
            rgba_stride_u16 = rgba_stride / 2;
            SliceStoreMut::Borrowed(casted)
        } else {
            is_owned_rgba = true;
            rgba_stride_u16 = width * 4;
            SliceStoreMut::Owned(vec![0u16; width as usize * 4 * height as usize])
        };

        let yuv_range = match range {
            YuvRange::Tv => yuvutils_rs::YuvRange::Limited,
            YuvRange::Pc => yuvutils_rs::YuvRange::Full,
        };

        let planar_image_with_alpha = YuvPlanarImage {
            y_plane: y_slice.0.as_ref(),
            y_stride: y_slice.1 as u32,
            u_plane: u_slice.0.as_ref(),
            u_stride: u_slice.1 as u32,
            v_plane: v_slice.0.as_ref(),
            v_stride: v_slice.1 as u32,
            width,
            height,
        };

        if yuv_matrix == YuvMatrix::Identity {
            assert_eq!(yuv_type, YuvType::Yuv444, "Identity exists only on 4:4:4");
            let dispatch = if bit_depth == 10 {
                gb10_to_rgba10
            } else {
                gb12_to_rgba12
            };
            dispatch(
                &planar_image_with_alpha,
                working_slice.borrow_mut(),
                rgba_stride_u16,
                yuv_range,
            )
            .unwrap();
        } else if yuv_matrix == YuvMatrix::YCgCo {
            let callee = match yuv_type {
                YuvType::Yuv420 => {
                    if bit_depth == 10 {
                        icgc010_to_rgba10
                    } else {
                        icgc012_to_rgba12
                    }
                }
                YuvType::Yuv422 => {
                    if bit_depth == 10 {
                        icgc210_to_rgba10
                    } else {
                        icgc212_to_rgba12
                    }
                }
                YuvType::Yuv444 => {
                    if bit_depth == 10 {
                        icgc410_to_rgba10
                    } else {
                        icgc412_to_rgba12
                    }
                }
            };

            callee(
                &planar_image_with_alpha,
                working_slice.borrow_mut(),
                rgba_stride_u16,
                yuv_range,
            )
            .unwrap();
        } else {
            let matrix = match yuv_matrix {
                YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
                YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
                YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
                YuvMatrix::Identity => unreachable!("Should be handled in another place"),
                YuvMatrix::YCgCo => unreachable!(),
            };

            let callee = match yuv_type {
                YuvType::Yuv420 => {
                    if bit_depth == 10 {
                        i010_to_rgba10
                    } else {
                        i012_to_rgba12
                    }
                }
                YuvType::Yuv422 => {
                    if bit_depth == 10 {
                        i210_to_rgba10
                    } else {
                        i212_to_rgba12
                    }
                }
                YuvType::Yuv444 => {
                    if bit_depth == 10 {
                        i410_to_rgba10
                    } else {
                        i412_to_rgba12
                    }
                }
            };

            callee(
                &planar_image_with_alpha,
                working_slice.borrow_mut(),
                rgba_stride_u16,
                yuv_range,
                matrix,
            )
            .unwrap();
        }

        let target =
            slice::from_raw_parts_mut(rgba as *mut u8, height as usize * rgba_stride as usize);
        if is_owned_rgba {
            for (dst, src) in target.chunks_exact_mut(rgba_stride as usize).zip(
                working_slice
                    .borrow()
                    .chunks_exact(rgba_stride_u16 as usize),
            ) {
                for (dst, src) in dst.chunks_exact_mut(2).zip(src.iter()) {
                    let bytes = src.to_ne_bytes();
                    dst[0] = bytes[0];
                    dst[1] = bytes[1];
                }
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn weave_yuv16_with_alpha_to_rgba16(
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
    unsafe {
        let y_slice = transmute_const_ptr16(
            y_plane,
            y_stride as usize,
            width as usize,
            height as usize,
            1,
        );
        let a_orig = transmute_const_ptr16(
            a_plane,
            a_stride as usize,
            width as usize,
            height as usize,
            1,
        );
        let chroma_height = if yuv_type == YuvType::Yuv420 {
            height.div_ceil(2) as usize
        } else {
            height as usize
        };
        let chroma_width = if yuv_type == YuvType::Yuv420 {
            width.div_ceil(2) as usize
        } else {
            width as usize
        };
        let u_slice =
            transmute_const_ptr16(u_plane, u_stride as usize, chroma_width, chroma_height, 1);
        let v_slice =
            transmute_const_ptr16(v_plane, v_stride as usize, chroma_width, chroma_height, 1);

        let rgba_slice8 =
            slice::from_raw_parts_mut(rgba as *mut u8, height as usize * rgba_stride as usize);
        let mut is_owned_rgba = false;
        let rgba_stride_u16;

        let mut working_slice = if let Ok(casted) = bytemuck::try_cast_slice_mut(rgba_slice8) {
            rgba_stride_u16 = rgba_stride / 2;
            SliceStoreMut::Borrowed(casted)
        } else {
            is_owned_rgba = true;
            rgba_stride_u16 = width * 4;
            SliceStoreMut::Owned(vec![0u16; width as usize * 4 * height as usize])
        };

        let yuv_range = match range {
            YuvRange::Tv => yuvutils_rs::YuvRange::Limited,
            YuvRange::Pc => yuvutils_rs::YuvRange::Full,
        };

        let planar_image_with_alpha = YuvPlanarImageWithAlpha {
            y_plane: y_slice.0.as_ref(),
            y_stride: y_slice.1 as u32,
            u_plane: u_slice.0.as_ref(),
            u_stride: u_slice.1 as u32,
            v_plane: v_slice.0.as_ref(),
            v_stride: v_slice.1 as u32,
            a_plane: a_orig.0.as_ref(),
            a_stride: a_orig.1 as u32,
            width,
            height,
        };

        if yuv_matrix == YuvMatrix::Identity {
            assert_eq!(
                yuv_type,
                YuvType::Yuv444,
                "Identity exists only in 4:4:4 layout"
            );
            let dispatch = if bit_depth == 10 {
                gb10_alpha_to_rgba10
            } else {
                gb12_alpha_to_rgba12
            };
            dispatch(
                &planar_image_with_alpha,
                working_slice.borrow_mut(),
                rgba_stride_u16,
                yuv_range,
            )
            .unwrap();
        } else if yuv_matrix == YuvMatrix::YCgCo {
            let callee = match yuv_type {
                YuvType::Yuv420 => {
                    if bit_depth == 10 {
                        icgc010_alpha_to_rgba10
                    } else {
                        icgc012_alpha_to_rgba12
                    }
                }
                YuvType::Yuv422 => {
                    if bit_depth == 10 {
                        icgc210_alpha_to_rgba10
                    } else {
                        icgc212_alpha_to_rgba12
                    }
                }
                YuvType::Yuv444 => {
                    if bit_depth == 10 {
                        icgc410_alpha_to_rgba10
                    } else {
                        icgc412_alpha_to_rgba12
                    }
                }
            };

            callee(
                &planar_image_with_alpha,
                working_slice.borrow_mut(),
                rgba_stride_u16,
                yuv_range,
            )
            .unwrap();
        } else {
            let matrix = match yuv_matrix {
                YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
                YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
                YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
                YuvMatrix::Identity => unreachable!("Should be handled in another place"),
                YuvMatrix::YCgCo => unreachable!("Should be handled in another place"),
            };

            let callee = match yuv_type {
                YuvType::Yuv420 => {
                    if bit_depth == 10 {
                        i010_alpha_to_rgba10
                    } else {
                        i012_alpha_to_rgba12
                    }
                }
                YuvType::Yuv422 => {
                    if bit_depth == 10 {
                        i210_alpha_to_rgba10
                    } else {
                        i212_alpha_to_rgba12
                    }
                }
                YuvType::Yuv444 => {
                    if bit_depth == 10 {
                        i410_alpha_to_rgba10
                    } else {
                        i412_alpha_to_rgba12
                    }
                }
            };

            callee(
                &planar_image_with_alpha,
                working_slice.borrow_mut(),
                rgba_stride_u16,
                yuv_range,
                matrix,
            )
            .unwrap();
        }

        let target =
            slice::from_raw_parts_mut(rgba as *mut u8, height as usize * rgba_stride as usize);
        if is_owned_rgba {
            for (dst, src) in target.chunks_exact_mut(rgba_stride as usize).zip(
                working_slice
                    .borrow()
                    .chunks_exact(rgba_stride_u16 as usize),
            ) {
                for (dst, src) in dst.chunks_exact_mut(2).zip(src.iter()) {
                    let bytes = src.to_ne_bytes();
                    dst[0] = bytes[0];
                    dst[1] = bytes[1];
                }
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn weave_scale_u8(
    src: *const u8,
    src_stride: u32,
    width: u32,
    height: u32,
    dst: *mut u8,
    dst_stride: u32,
    new_width: u32,
    new_height: u32,
    method: u32,
    premultiply_alpha: bool,
) {
    unsafe {
        let origin_slice = slice::from_raw_parts(src, src_stride as usize * height as usize);
        let dst_slice = slice::from_raw_parts_mut(dst, dst_stride as usize * new_height as usize);

        let source_store = ImageStore::<u8, 4> {
            buffer: std::borrow::Cow::Borrowed(origin_slice),
            channels: 4,
            width: width as usize,
            height: height as usize,
            stride: src_stride as usize,
            bit_depth: 8,
        };

        let mut scaler = Scaler::new(if method == 3 {
            ResamplingFunction::Lanczos3
        } else if method == 1 {
            ResamplingFunction::Nearest
        } else {
            ResamplingFunction::Bilinear
        });

        scaler.set_threading_policy(ThreadingPolicy::Adaptive);

        let mut dst_store = ImageStoreMut::<u8, 4> {
            buffer: BufferStore::Borrowed(dst_slice),
            channels: 4,
            width: new_width as usize,
            height: new_height as usize,
            stride: dst_stride as usize,
            bit_depth: 8,
        };

        scaler
            .resize_rgba(&source_store, &mut dst_store, premultiply_alpha)
            .unwrap();
    }
}

#[no_mangle]
pub extern "C" fn weave_scale_u16(
    src: *const u16,
    src_stride: usize,
    width: u32,
    height: u32,
    dst: *mut u16,
    new_width: u32,
    new_height: u32,
    bit_depth: usize,
    method: u32,
    premultiply_alpha: bool,
) {
    unsafe {
        let source_image: std::borrow::Cow<[u16]>;
        let mut j_src_stride = src_stride / 2;

        if src as usize % 2 != 0 || src_stride % 2 != 0 {
            let mut _src_slice = vec![0u16; width as usize * height as usize * 4];
            let j = slice::from_raw_parts(src as *const u8, src_stride * height as usize);

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
            source_image = std::borrow::Cow::Borrowed(slice::from_raw_parts(
                src,
                src_stride / 2 * height as usize,
            ));
        }

        let _source_store = ImageStore::<u16, 4> {
            buffer: source_image,
            channels: 4,
            width: width as usize,
            height: height as usize,
            stride: j_src_stride,
            bit_depth,
        };

        let mut scaler = Scaler::new(if method == 3 {
            ResamplingFunction::Lanczos3
        } else if method == 1 {
            ResamplingFunction::Nearest
        } else {
            ResamplingFunction::Bilinear
        });
        scaler.set_threading_policy(ThreadingPolicy::Adaptive);
        scaler.set_workload_strategy(WorkloadStrategy::PreferQuality);

        if dst as usize % 2 != 0 {
            let mut dst_store =
                ImageStoreMut::alloc_with_depth(new_width as usize, new_height as usize, bit_depth);

            scaler
                .resize_rgba_u16(&_source_store, &mut dst_store, premultiply_alpha)
                .unwrap();

            let dst_slice = slice::from_raw_parts_mut(
                dst as *mut u8,
                new_width as usize * 4 * new_height as usize,
            );

            for (src, dst) in dst_store
                .as_bytes()
                .chunks_exact(dst_store.stride())
                .zip(dst_slice.chunks_exact_mut(new_width as usize * 4))
            {
                for (src, dst) in src.iter().zip(dst.chunks_exact_mut(4)) {
                    let dst_ptr = dst.as_mut_ptr() as *mut u16;
                    dst_ptr.write_unaligned(*src);
                }
            }
        } else {
            let dst_stride =
                std::slice::from_raw_parts_mut(dst, new_height as usize * new_width as usize * 4);
            let buffer = BufferStore::Borrowed(dst_stride);
            let mut dst_store = ImageStoreMut::<u16, 4> {
                buffer,
                width: new_width as usize,
                height: new_height as usize,
                bit_depth,
                channels: 4,
                stride: new_width as usize * 4,
            };

            scaler
                .resize_rgba_u16(&_source_store, &mut dst_store, premultiply_alpha)
                .unwrap();
        }
    }
}

use core::f16;

#[no_mangle]
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

        if src as usize % 2 != 0 || src_stride % 2 != 0 {
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

        let mut scaler = Scaler::new(if method == 3 {
            ResamplingFunction::Lanczos3
        } else if method == 1 {
            ResamplingFunction::Nearest
        } else {
            ResamplingFunction::Bilinear
        });
        scaler.set_threading_policy(ThreadingPolicy::Adaptive);
        scaler.set_workload_strategy(WorkloadStrategy::PreferQuality);

        if dst as usize % 2 != 0 {
            let mut dst_store =
                ImageStoreMut::alloc_with_depth(new_width as usize, new_height as usize, 10);

            scaler
                .resize_rgba_f16(&_source_store, &mut dst_store, premultiply_alpha)
                .unwrap();

            let dst_slice = slice::from_raw_parts_mut(
                dst as *mut u8,
                new_width as usize * 4 * new_height as usize,
            );

            for (src, dst) in dst_store
                .as_bytes()
                .chunks_exact(dst_store.stride())
                .zip(dst_slice.chunks_exact_mut(new_width as usize * 4))
            {
                for (src, dst) in src.iter().zip(dst.chunks_exact_mut(4)) {
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

            scaler
                .resize_rgba_f16(&_source_store, &mut dst_store, premultiply_alpha)
                .unwrap();
        }
    }
}

#[no_mangle]
pub extern "C" fn weave_rgba_to_yuv(
    rgba: *const u8,
    rgba_stride: u32,
    y_plane: *mut u8,
    y_stride: u32,
    u_plane: *mut u8,
    u_stride: u32,
    v_plane: *mut u8,
    v_stride: u32,
    width: u32,
    height: u32,
    range: YuvRange,
    matrix: YuvMatrix,
    yuv_type: YuvType,
) {
    unsafe {
        let rgba_slice = std::slice::from_raw_parts(rgba, rgba_stride as usize * height as usize);
        let y_slice = std::slice::from_raw_parts_mut(y_plane, y_stride as usize * height as usize);
        let u_slice = if yuv_type == YuvType::Yuv420 {
            slice::from_raw_parts_mut(u_plane, u_stride as usize * (height as usize).div_ceil(2))
        } else {
            slice::from_raw_parts_mut(u_plane, u_stride as usize * height as usize)
        };
        let v_slice = if yuv_type == YuvType::Yuv420 {
            slice::from_raw_parts_mut(v_plane, v_stride as usize * (height as usize).div_ceil(2))
        } else {
            slice::from_raw_parts_mut(v_plane, v_stride as usize * height as usize)
        };

        let mut planar_image = YuvPlanarImageMut {
            y_plane: BufferStoreMut::Borrowed(y_slice),
            y_stride,
            u_plane: BufferStoreMut::Borrowed(u_slice),
            u_stride,
            v_plane: BufferStoreMut::Borrowed(v_slice),
            v_stride,
            width,
            height,
        };

        let yuv_range = match range {
            YuvRange::Tv => yuvutils_rs::YuvRange::Limited,
            YuvRange::Pc => yuvutils_rs::YuvRange::Full,
        };

        if matrix == YuvMatrix::Identity {
            rgba_to_gbr(&mut planar_image, rgba_slice, rgba_stride, yuv_range).unwrap();
        } else if matrix == YuvMatrix::YCgCo {
            let worker = match yuv_type {
                YuvType::Yuv420 => rgba_to_ycgco420,
                YuvType::Yuv422 => rgba_to_ycgco422,
                YuvType::Yuv444 => rgba_to_ycgco444,
            };

            worker(&mut planar_image, rgba_slice, rgba_stride, yuv_range).unwrap();
        } else {
            let matrix = match matrix {
                YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
                YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
                YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
                YuvMatrix::Identity => unreachable!("Should be handled in another place"),
                YuvMatrix::YCgCo => unreachable!("Should be handled in another place"),
            };

            let worker = match yuv_type {
                YuvType::Yuv420 => rgba_to_yuv420,
                YuvType::Yuv422 => rgba_to_yuv422,
                YuvType::Yuv444 => rgba_to_yuv444,
            };

            worker(
                &mut planar_image,
                rgba_slice,
                rgba_stride,
                yuv_range,
                matrix,
                YuvConversionMode::Balanced,
            )
            .unwrap();
        }
    }
}

#[no_mangle]
pub extern "C" fn weave_rgba_to_yuv400(
    rgba: *const u8,
    rgba_stride: u32,
    y_plane: *mut u8,
    y_stride: u32,
    width: u32,
    height: u32,
    range: YuvRange,
    yuv_matrix: YuvMatrix,
) {
    unsafe {
        let rgba_slice = std::slice::from_raw_parts(rgba, rgba_stride as usize * height as usize);
        let y_slice = std::slice::from_raw_parts_mut(y_plane, y_stride as usize * height as usize);

        let mut planar_image = YuvGrayImageMut {
            y_plane: BufferStoreMut::Borrowed(y_slice),
            y_stride,
            width,
            height,
        };

        let yuv_range = match range {
            YuvRange::Tv => yuvutils_rs::YuvRange::Limited,
            YuvRange::Pc => yuvutils_rs::YuvRange::Full,
        };

        let matrix = match yuv_matrix {
            YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
            YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
            YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
            YuvMatrix::Identity => unreachable!("Should be handled in another place"),
            YuvMatrix::YCgCo => unreachable!("Should be handled in another place"),
        };

        rgba_to_yuv400(
            &mut planar_image,
            rgba_slice,
            rgba_stride,
            yuv_range,
            matrix,
        )
        .unwrap();
    }
}

#[no_mangle]
pub extern "C" fn weave_yuv16_to_rgba_f16(
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
    unsafe {
        let y_slice = transmute_const_ptr16(
            y_plane,
            y_stride as usize,
            width as usize,
            height as usize,
            1,
        );
        let chroma_height = if yuv_type == YuvType::Yuv420 {
            height.div_ceil(2) as usize
        } else {
            height as usize
        };
        let chroma_width = if yuv_type == YuvType::Yuv420 {
            width.div_ceil(2) as usize
        } else {
            width as usize
        };
        let u_slice =
            transmute_const_ptr16(u_plane, u_stride as usize, chroma_width, chroma_height, 1);
        let v_slice =
            transmute_const_ptr16(v_plane, v_stride as usize, chroma_width, chroma_height, 1);

        let rgba_slice8 =
            slice::from_raw_parts_mut(rgba as *mut u8, height as usize * rgba_stride as usize);

        let mut is_owned_rgba = false;
        let rgba_stride_u16;

        let mut working_slice = if let Ok(casted) = bytemuck::try_cast_slice_mut(rgba_slice8) {
            rgba_stride_u16 = rgba_stride / 2;
            SliceStoreMut::Borrowed(casted)
        } else {
            is_owned_rgba = true;
            rgba_stride_u16 = width * 4;
            SliceStoreMut::Owned(vec![0_f16; width as usize * 4 * height as usize])
        };

        let yuv_range = match range {
            YuvRange::Tv => yuvutils_rs::YuvRange::Limited,
            YuvRange::Pc => yuvutils_rs::YuvRange::Full,
        };

        let planar_image_with_alpha = YuvPlanarImage {
            y_plane: y_slice.0.as_ref(),
            y_stride: y_slice.1 as u32,
            u_plane: u_slice.0.as_ref(),
            u_stride: u_slice.1 as u32,
            v_plane: v_slice.0.as_ref(),
            v_stride: v_slice.1 as u32,
            width,
            height,
        };

        if yuv_matrix == YuvMatrix::Identity {
            assert_eq!(yuv_type, YuvType::Yuv444, "Identity exists only on 4:4:4");
            let dispatch = if bit_depth == 10 {
                gb10_to_rgba_f16
            } else {
                gb12_to_rgba_f16
            };
            dispatch(
                &planar_image_with_alpha,
                bytemuck::cast_slice_mut(working_slice.borrow_mut()),
                rgba_stride_u16,
                yuv_range,
            )
            .unwrap();
        } else if yuv_matrix == YuvMatrix::YCgCo {
            let mut working_target = vec![0u16; width as usize * height as usize * 4];
            let callee = match yuv_type {
                YuvType::Yuv420 => {
                    if bit_depth == 10 {
                        icgc010_to_rgba10
                    } else {
                        icgc012_to_rgba12
                    }
                }
                YuvType::Yuv422 => {
                    if bit_depth == 10 {
                        icgc210_to_rgba10
                    } else {
                        icgc212_to_rgba12
                    }
                }
                YuvType::Yuv444 => {
                    if bit_depth == 10 {
                        icgc410_to_rgba10
                    } else {
                        icgc412_to_rgba12
                    }
                }
            };

            callee(
                &planar_image_with_alpha,
                &mut working_target,
                rgba_stride_u16,
                yuv_range,
            )
            .unwrap();
            convert_rgba16_to_f16(
                &working_target,
                width as usize * 4,
                working_slice.borrow_mut(),
                rgba_stride_u16 as usize,
                bit_depth as usize,
                width as usize,
                height as usize,
            )
            .unwrap();
        } else {
            let matrix = match yuv_matrix {
                YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
                YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
                YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
                YuvMatrix::Identity => unreachable!("Should be handled in another place"),
                YuvMatrix::YCgCo => unreachable!(),
            };

            let callee = match yuv_type {
                YuvType::Yuv420 => {
                    if bit_depth == 10 {
                        i010_to_rgba_f16
                    } else {
                        i012_to_rgba_f16
                    }
                }
                YuvType::Yuv422 => {
                    if bit_depth == 10 {
                        i210_to_rgba_f16
                    } else {
                        i212_to_rgba_f16
                    }
                }
                YuvType::Yuv444 => {
                    if bit_depth == 10 {
                        i410_to_rgba_f16
                    } else {
                        i412_to_rgba_f16
                    }
                }
            };

            callee(
                &planar_image_with_alpha,
                working_slice.borrow_mut(),
                rgba_stride_u16,
                yuv_range,
                matrix,
            )
            .unwrap();
        }

        if is_owned_rgba {
            let target =
                slice::from_raw_parts_mut(rgba as *mut u8, height as usize * rgba_stride as usize);
            for (dst, src) in target.chunks_exact_mut(rgba_stride as usize).zip(
                working_slice
                    .borrow()
                    .chunks_exact(rgba_stride_u16 as usize),
            ) {
                for (dst, src) in dst.chunks_exact_mut(2).zip(src.iter()) {
                    let bytes = src.to_bits().to_ne_bytes();
                    dst[0] = bytes[0];
                    dst[1] = bytes[1];
                }
            }
        }
    }
}
