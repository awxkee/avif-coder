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

mod support;

use crate::support::{transmute_const_ptr16, SliceStoreMut};
use pic_scale::{
    ImageSize, ImageStore, ResamplingFunction, Scaler, Scaling, ScalingU16, ThreadingPolicy,
};
use std::fmt::Debug;
use std::slice;
use yuvutils_rs::{
    gbr_to_rgba, gbr_to_rgba_p16, gbr_with_alpha_to_rgba, gbr_with_alpha_to_rgba_p16,
    yuv400_p16_to_rgba16, yuv400_p16_with_alpha_to_rgba16, yuv400_to_rgba,
    yuv400_with_alpha_to_rgba, yuv420_p16_to_rgba16, yuv420_p16_with_alpha_to_rgba16,
    yuv420_to_rgba, yuv420_with_alpha_to_rgba, yuv422_p16_to_rgba16,
    yuv422_p16_with_alpha_to_rgba16, yuv422_to_rgba, yuv422_with_alpha_to_rgba,
    yuv444_p16_to_rgba16, yuv444_p16_with_alpha_to_rgba16, yuv444_to_rgba,
    yuv444_with_alpha_to_rgba, YuvBytesPacking, YuvEndianness, YuvGrayAlphaImage, YuvGrayImage,
    YuvPlanarImage, YuvPlanarImageWithAlpha, YuvStandardMatrix,
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
    Identity,
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
        } else {
            let matrix = match yuv_matrix {
                YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
                YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
                YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
                YuvMatrix::Identity => unreachable!(),
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
        };

        yuv400_with_alpha_to_rgba(&gray_with_alpha, rgba_slice, rgba_stride, yuv_range, matrix)
            .unwrap();
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
        };

        let gray_image = YuvGrayImage {
            y_plane: y_slice.0.as_ref(),
            y_stride: y_slice.1 as u32,
            width,
            height,
        };

        yuv400_p16_to_rgba16(
            &gray_image,
            working_slice.borrow_mut(),
            rgba_stride_u16,
            bit_depth,
            yuv_range,
            matrix,
            YuvEndianness::LittleEndian,
            YuvBytesPacking::LeastSignificantBytes,
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

        yuv400_p16_with_alpha_to_rgba16(
            &gray_alpha,
            working_slice.borrow_mut(),
            rgba_stride_u16,
            bit_depth,
            yuv_range,
            matrix,
            YuvEndianness::LittleEndian,
            YuvBytesPacking::LeastSignificantBytes,
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
        } else {
            let matrix = match yuv_matrix {
                YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
                YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
                YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
                YuvMatrix::Identity => unreachable!("Should be handled in another place"),
            };

            let callee = match yuv_type {
                YuvType::Yuv420 => yuv420_with_alpha_to_rgba,
                YuvType::Yuv422 => yuv422_with_alpha_to_rgba,
                YuvType::Yuv444 => yuv444_with_alpha_to_rgba,
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
            ((height + 1) / 2) as usize
        } else {
            height as usize
        };
        let chroma_width = if yuv_type == YuvType::Yuv420 {
            (width as usize + 1) / 2
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
            gbr_to_rgba_p16(
                &planar_image_with_alpha,
                working_slice.borrow_mut(),
                rgba_stride_u16,
                bit_depth,
                yuv_range,
            )
            .unwrap();
        } else {
            let matrix = match yuv_matrix {
                YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
                YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
                YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
                YuvMatrix::Identity => unreachable!("Should be handled in another place"),
            };

            let callee = match yuv_type {
                YuvType::Yuv420 => yuv420_p16_to_rgba16,
                YuvType::Yuv422 => yuv422_p16_to_rgba16,
                YuvType::Yuv444 => yuv444_p16_to_rgba16,
            };

            callee(
                &planar_image_with_alpha,
                working_slice.borrow_mut(),
                rgba_stride_u16,
                bit_depth as usize,
                yuv_range,
                matrix,
                YuvEndianness::LittleEndian,
                YuvBytesPacking::LeastSignificantBytes,
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
            ((height + 1) / 2) as usize
        } else {
            height as usize
        };
        let chroma_width = if yuv_type == YuvType::Yuv420 {
            (width as usize + 1) / 2
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
            gbr_with_alpha_to_rgba_p16(
                &planar_image_with_alpha,
                working_slice.borrow_mut(),
                rgba_stride_u16,
                bit_depth,
                yuv_range,
            )
            .unwrap();
        } else {
            let matrix = match yuv_matrix {
                YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
                YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
                YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
                YuvMatrix::Identity => unreachable!("Should be handled in another place"),
            };

            let callee = match yuv_type {
                YuvType::Yuv420 => yuv420_p16_with_alpha_to_rgba16,
                YuvType::Yuv422 => yuv422_p16_with_alpha_to_rgba16,
                YuvType::Yuv444 => yuv444_p16_with_alpha_to_rgba16,
            };

            callee(
                &planar_image_with_alpha,
                working_slice.borrow_mut(),
                rgba_stride_u16,
                bit_depth as usize,
                yuv_range,
                matrix,
                YuvEndianness::LittleEndian,
                YuvBytesPacking::LeastSignificantBytes,
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
    dst: *const u8,
    dst_stride: u32,
    new_width: u32,
    new_height: u32,
    method: u32,
    premultiply_alpha: bool,
) {
    unsafe {
        let mut src_slice = vec![0u8; width as usize * height as usize * 4];
        let origin_slice = slice::from_raw_parts(src, src_stride as usize * height as usize);

        for (dst, src) in src_slice
            .chunks_exact_mut(width as usize * 4)
            .zip(origin_slice.chunks_exact(src_stride as usize))
        {
            for (dst, &src) in dst.iter_mut().zip(src.iter()) {
                *dst = src;
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

        let new_store = scaler
            .resize_rgba(
                ImageSize::new(new_width as usize, new_height as usize),
                source_store,
                premultiply_alpha,
            )
            .unwrap();

        let dst_slice =
            slice::from_raw_parts_mut(dst as *mut u8, dst_stride as usize * new_height as usize);

        for (dst_chunk, src_chunk) in dst_slice
            .chunks_mut(dst_stride as usize)
            .zip(new_store.as_bytes().chunks(new_width as usize * 4))
        {
            for (dst, src) in dst_chunk.iter_mut().zip(src_chunk.iter()) {
                *dst = *src;
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn weave_scale_u16(
    src: *const u16,
    src_stride: usize,
    width: u32,
    height: u32,
    dst: *const u16,
    new_width: u32,
    new_height: u32,
    bit_depth: usize,
    method: u32,
    premultiply_alpha: bool,
) {
    unsafe {
        let mut _src_slice = vec![0u16; width as usize * height as usize * 4];

        let source_image = slice::from_raw_parts(src as *const u8, src_stride * height as usize);

        for (dst, src) in _src_slice
            .chunks_exact_mut(width as usize * 4)
            .zip(source_image.chunks_exact(src_stride))
        {
            for (dst, src) in dst.iter_mut().zip(src.chunks_exact(2)) {
                let pixel = u16::from_ne_bytes([src[0], src[1]]);
                *dst = pixel;
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

        let _new_store = scaler
            .resize_rgba_u16(
                ImageSize::new(new_width as usize, new_height as usize),
                _source_store,
                bit_depth,
                premultiply_alpha,
            )
            .unwrap();
        let dst_slice = slice::from_raw_parts_mut(
            dst as *mut u16,
            new_width as usize * 4 * new_height as usize,
        );
        for (src, dst) in _new_store.as_bytes().iter().zip(dst_slice.iter_mut()) {
            *dst = *src;
        }
    }
}
