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
use core::f16;
use yuvutils_rs::{
    convert_rgba16_to_f16, convert_rgba_to_f16, rgba10_to_ar30, rgba12_to_ar30, rgba8_to_ar30,
    BufferStoreMut, Rgb30ByteOrder,
};

#[inline(always)]
fn work_on_transmuted_ptr_f16<F>(
    rgba: *mut u16,
    rgba_stride: u32,
    width: usize,
    height: usize,
    copy: bool,
    lambda: F,
) where
    F: Fn(&mut [f16], usize),
{
    let mut allocated = false;
    let mut dst_stride = rgba_stride as usize / 2;
    let mut working_slice: BufferStoreMut<'_, f16> = unsafe {
        if rgba as usize % 2 == 0 && rgba_stride / 2 == 0 {
            BufferStoreMut::Borrowed(std::slice::from_raw_parts_mut(
                rgba as *mut f16,
                rgba_stride as usize / 2 * height,
            ))
        } else {
            allocated = true;
            dst_stride = width * 4;

            let mut dst_slice = vec![0.; width * height * 4];
            if copy {
                let src_slice =
                    std::slice::from_raw_parts(rgba as *mut u8, rgba_stride as usize * height);
                for (dst, src) in dst_slice
                    .chunks_exact_mut(dst_stride)
                    .zip(src_slice.chunks_exact(rgba_stride as usize))
                {
                    let src = &src[0..width * 4 * 2];
                    let dst = &mut dst[0..width * 4];
                    for (dst, src) in dst.iter_mut().zip(src.chunks_exact(2)) {
                        *dst = f16::from_bits(u16::from_ne_bytes([src[0], src[1]]));
                    }
                }
            }
            BufferStoreMut::Owned(dst_slice)
        }
    };

    lambda(working_slice.borrow_mut(), dst_stride);

    if allocated {
        let src_slice = working_slice.borrow();
        let dst_slice = unsafe {
            std::slice::from_raw_parts_mut(rgba as *mut u8, rgba_stride as usize * height)
        };
        for (src, dst) in src_slice
            .chunks_exact(dst_stride)
            .zip(dst_slice.chunks_exact_mut(rgba_stride as usize))
        {
            let src = &src[0..width * 4];
            let dst = &mut dst[0..width * 4 * 2];
            for (src, dst) in src.iter().zip(dst.chunks_exact_mut(2)) {
                let bytes = src.to_ne_bytes();
                dst[0] = bytes[0];
                dst[1] = bytes[1];
            }
        }
    }
}

#[inline(always)]
pub(crate) fn work_on_transmuted_ptr_u16<F>(
    rgba: *const u16,
    rgba_stride: u32,
    width: usize,
    height: usize,
    copy: bool,
    lambda: F,
) where
    F: Fn(&mut [u16], usize),
{
    let mut allocated = false;
    let mut dst_stride = rgba_stride as usize / 2;
    let mut working_slice: BufferStoreMut<'_, u16> = unsafe {
        if rgba as usize % 2 == 0 && rgba_stride / 2 == 0 {
            BufferStoreMut::Borrowed(std::slice::from_raw_parts_mut(
                rgba as *mut u16,
                rgba_stride as usize / 2 * height,
            ))
        } else {
            allocated = true;
            dst_stride = width * 4;

            let mut dst_slice = vec![0; width * height * 4];
            if copy {
                let src_slice =
                    std::slice::from_raw_parts(rgba as *mut u8, rgba_stride as usize * height);
                for (dst, src) in dst_slice
                    .chunks_exact_mut(dst_stride)
                    .zip(src_slice.chunks_exact(rgba_stride as usize))
                {
                    let src = &src[0..width * 4 * 2];
                    let dst = &mut dst[0..width * 4];
                    for (dst, src) in dst.iter_mut().zip(src.chunks_exact(2)) {
                        *dst = u16::from_ne_bytes([src[0], src[1]]);
                    }
                }
            }
            BufferStoreMut::Owned(dst_slice)
        }
    };

    lambda(working_slice.borrow_mut(), dst_stride);

    if allocated {
        let src_slice = working_slice.borrow();
        let dst_slice = unsafe {
            std::slice::from_raw_parts_mut(rgba as *mut u8, rgba_stride as usize * height)
        };
        for (src, dst) in src_slice
            .chunks_exact(dst_stride)
            .zip(dst_slice.chunks_exact_mut(rgba_stride as usize))
        {
            let src = &src[0..width * 4];
            let dst = &mut dst[0..width * 4 * 2];
            for (src, dst) in src.iter().zip(dst.chunks_exact_mut(2)) {
                let bytes = src.to_ne_bytes();
                dst[0] = bytes[0];
                dst[1] = bytes[1];
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn weave_cvt_rgba8_to_rgba_f16(
    rgba8: *const u8,
    rgba8_stride: u32,
    rgba_f16: *mut u16,
    rgba_f16_stride: u32,
    width: u32,
    height: u32,
) {
    work_on_transmuted_ptr_f16(
        rgba_f16,
        rgba_f16_stride,
        width as usize,
        height as usize,
        false,
        |x: &mut [f16], dst_stride: usize| {
            let src_slice = unsafe {
                std::slice::from_raw_parts(rgba8, rgba8_stride as usize * height as usize)
            };
            convert_rgba_to_f16(
                src_slice,
                rgba8_stride as usize,
                x,
                dst_stride,
                width as usize,
                height as usize,
            )
            .unwrap();
        },
    );
}

#[no_mangle]
pub extern "C" fn weave_premultiply_rgba_f16(
    rgba_f16: *mut u16,
    rgba_f16_stride: u32,
    width: u32,
    height: u32,
) {
    work_on_transmuted_ptr_f16(
        rgba_f16,
        rgba_f16_stride,
        width as usize,
        height as usize,
        true,
        |x: &mut [f16], dst_stride: usize| {
            #[cfg(target_arch = "aarch64")]
            {
                if std::arch::is_aarch64_feature_detected!("fp16") {
                    unsafe { premultiply_default_aarch64_fp_16(x, dst_stride) }
                } else {
                    premultiply_default(x, dst_stride)
                }
            }
            #[cfg(not(target_arch = "aarch64"))]
            {
                premultiply_default(x, dst_stride)
            }
        },
    );
}

fn premultiply_default(x: &mut [f16], dst_stride: usize) {
    for lane in x.chunks_exact_mut(dst_stride) {
        for chunk in lane.chunks_exact_mut(4) {
            let a = chunk[3];
            chunk[0] *= a;
            chunk[1] *= a;
            chunk[2] *= a;
        }
    }
}

#[cfg(target_arch = "aarch64")]
#[target_feature(enable = "fp16")]
unsafe fn premultiply_default_aarch64_fp_16(x: &mut [f16], dst_stride: usize) {
    for lane in x.chunks_exact_mut(dst_stride) {
        for chunk in lane.chunks_exact_mut(4) {
            let a = chunk[3];
            chunk[0] *= a;
            chunk[1] *= a;
            chunk[2] *= a;
        }
    }
}

#[no_mangle]
pub extern "C" fn weave_cvt_rgba8_to_ar30(
    rgba8: *const u8,
    rgba8_stride: u32,
    ar30: *mut u8,
    ar30_stride: u32,
    width: u32,
    height: u32,
) {
    let src_slice =
        unsafe { std::slice::from_raw_parts(rgba8, rgba8_stride as usize * height as usize) };
    let mut dst_slice =
        unsafe { std::slice::from_raw_parts_mut(ar30, ar30_stride as usize * height as usize) };
    rgba8_to_ar30(
        &mut dst_slice,
        ar30_stride,
        Rgb30ByteOrder::Host,
        &src_slice,
        rgba8_stride,
        width,
        height,
    )
    .unwrap();
}

#[no_mangle]
pub extern "C" fn weave_cvt_rgba16_to_ar30(
    rgba16: *const u16,
    rgba16_stride: u32,
    bit_depth: usize,
    ar30: *mut u8,
    ar30_stride: u32,
    width: u32,
    height: u32,
) {
    work_on_transmuted_ptr_u16(
        rgba16,
        rgba16_stride,
        width as usize,
        height as usize,
        true,
        |src: &mut [u16], src_stride: usize| {
            let mut dst_slice = unsafe {
                std::slice::from_raw_parts_mut(ar30, ar30_stride as usize * height as usize)
            };
            let handle = if bit_depth == 10 {
                rgba10_to_ar30
            } else {
                rgba12_to_ar30
            };
            handle(
                &mut dst_slice,
                ar30_stride,
                Rgb30ByteOrder::Host,
                &src,
                src_stride as u32,
                width,
                height,
            )
            .unwrap();
        },
    );
}

#[no_mangle]
pub extern "C" fn weave_cvt_rgba16_to_rgba_f16(
    rgba16: *const u16,
    rgba16_stride: u32,
    bit_depth: usize,
    rgba_f16: *mut u16,
    rgba_f16_stride: u32,
    width: u32,
    height: u32,
) {
    work_on_transmuted_ptr_u16(
        rgba16,
        rgba16_stride,
        width as usize,
        height as usize,
        true,
        |src: &mut [u16], src_stride: usize| {
            work_on_transmuted_ptr_f16(
                rgba_f16,
                rgba_f16_stride,
                width as usize,
                height as usize,
                false,
                |dst: &mut [f16], dst_stride: usize| {
                    convert_rgba16_to_f16(
                        src,
                        src_stride,
                        dst,
                        dst_stride,
                        bit_depth,
                        width as usize,
                        height as usize,
                    )
                    .unwrap();
                },
            );
        },
    );
}
