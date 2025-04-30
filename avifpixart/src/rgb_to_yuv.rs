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
use crate::{YuvMatrix, YuvRange, YuvType};
use std::slice;
use yuv::{
    rgba_to_gbr, rgba_to_ycgco420, rgba_to_ycgco422, rgba_to_ycgco444, rgba_to_yuv400,
    rgba_to_yuv420, rgba_to_yuv422, rgba_to_yuv444, BufferStoreMut, YuvConversionMode,
    YuvGrayImageMut, YuvPlanarImageMut, YuvStandardMatrix,
};

#[no_mangle]
pub extern "C" fn weave_rgba8_to_yuv8(
    y_plane: *mut u8,
    y_stride: u32,
    u_plane: *mut u8,
    u_stride: u32,
    v_plane: *mut u8,
    v_stride: u32,
    rgba: *const u8,
    rgba_stride: u32,
    width: u32,
    height: u32,
    range: YuvRange,
    yuv_matrix: YuvMatrix,
    yuv_type: YuvType,
) {
    unsafe {
        let y_slice = slice::from_raw_parts_mut(y_plane, y_stride as usize * height as usize);
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
        let rgba_slice = slice::from_raw_parts(rgba, height as usize * rgba_stride as usize);

        let yuv_range = match range {
            YuvRange::Tv => yuv::YuvRange::Limited,
            YuvRange::Pc => yuv::YuvRange::Full,
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

        if yuv_matrix == YuvMatrix::Identity {
            assert_eq!(yuv_type, YuvType::Yuv444, "Identity exists only on 4:4:4");
            rgba_to_gbr(&mut planar_image, rgba_slice, rgba_stride, yuv_range).unwrap();
        } else if yuv_matrix == YuvMatrix::YCgCo {
            let callee = match yuv_type {
                YuvType::Yuv420 => rgba_to_ycgco420,
                YuvType::Yuv422 => rgba_to_ycgco422,
                YuvType::Yuv444 => rgba_to_ycgco444,
            };
            callee(&mut planar_image, rgba_slice, rgba_stride, yuv_range).unwrap();
        } else {
            let matrix = match yuv_matrix {
                YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
                YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
                YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
                YuvMatrix::Identity => unreachable!(),
                YuvMatrix::YCgCo => unreachable!(),
            };

            let callee = match yuv_type {
                YuvType::Yuv420 => rgba_to_yuv420,
                YuvType::Yuv422 => rgba_to_yuv422,
                YuvType::Yuv444 => rgba_to_yuv444,
            };

            callee(
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
pub extern "C" fn weave_rgba8_to_y08(
    y_plane: *mut u8,
    y_stride: u32,
    rgba: *const u8,
    rgba_stride: u32,
    width: u32,
    height: u32,
    range: YuvRange,
    yuv_matrix: YuvMatrix,
) {
    unsafe {
        let y_slice = slice::from_raw_parts_mut(y_plane, y_stride as usize * height as usize);
        let rgba_slice = slice::from_raw_parts(rgba, height as usize * rgba_stride as usize);

        let yuv_range = match range {
            YuvRange::Tv => yuv::YuvRange::Limited,
            YuvRange::Pc => yuv::YuvRange::Full,
        };

        let mut planar_image = YuvGrayImageMut {
            y_plane: BufferStoreMut::Borrowed(y_slice),
            y_stride,
            width,
            height,
        };

        let matrix = match yuv_matrix {
            YuvMatrix::Bt601 => YuvStandardMatrix::Bt601,
            YuvMatrix::Bt709 => YuvStandardMatrix::Bt709,
            YuvMatrix::Bt2020 => YuvStandardMatrix::Bt2020,
            YuvMatrix::Identity => unreachable!(),
            YuvMatrix::YCgCo => unreachable!(),
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

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn test_yuv_encoding422() {
        let width = 550;
        let height = 333;
        let mut y_0 = vec![0u8; width * height];
        let mut u = vec![0u8; 275 * height.div_ceil(2)];
        let mut v = vec![0u8; 275 * height.div_ceil(2)];
        let rgba = vec![0u8; width * height * 4];
        weave_rgba8_to_yuv8(
            y_0.as_mut_ptr(),
            width as u32,
            u.as_mut_ptr(),
            275,
            v.as_mut_ptr(),
            275,
            rgba.as_ptr(),
            width as u32 * 4,
            width as u32,
            height as u32,
            YuvRange::Pc,
            YuvMatrix::Bt601,
            YuvType::Yuv422,
        );
    }
}
