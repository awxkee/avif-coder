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
use crate::cvt::work_on_transmuted_ptr_u16;
use moxcms::{ColorProfile, Layout, TransformOptions};

#[no_mangle]
pub unsafe extern "C" fn apply_icc_rgba8(
    src_image: *const u8,
    src_stride: u32,
    dst_image: *mut u8,
    dst_stride: u32,
    width: u32,
    height: u32,
    icc_profile: *const u8,
    icc_profile_stride: u32,
) {
    unsafe {
        let icc_data = std::slice::from_raw_parts(icc_profile, icc_profile_stride as usize);
        let src_image =
            std::slice::from_raw_parts(src_image, src_stride as usize * height as usize);
        let dst_image =
            std::slice::from_raw_parts_mut(dst_image, dst_stride as usize * height as usize);
        match ColorProfile::new_from_slice(&icc_data) {
            Ok(icc_profile) => {
                apply_icc_rgba8_impl(
                    src_image,
                    src_stride,
                    dst_image,
                    dst_stride,
                    width,
                    icc_profile,
                );
            }
            Err(_) => {
                straight_copy(src_stride, dst_stride, width, src_image, dst_image, 1);
            }
        };
    }
}

pub(crate) fn apply_icc_rgba8_impl(
    src_image: &[u8],
    src_stride: u32,
    dst_image: &mut [u8],
    dst_stride: u32,
    width: u32,
    profile: ColorProfile,
) {
    let dst_profile = ColorProfile::new_srgb();
    let transform = profile.create_transform_8bit(
        Layout::Rgba,
        &dst_profile,
        Layout::Rgba,
        TransformOptions::default(),
    );
    match transform {
        Ok(transform) => {
            for (src_row, dst_row) in src_image
                .chunks_exact(src_stride as usize)
                .zip(dst_image.chunks_exact_mut(dst_stride as usize))
            {
                let src = &src_row[..width as usize * 4];
                let dst = &mut dst_row[..width as usize * 4];
                transform.transform(src, dst).unwrap();
            }
        }
        Err(_) => {
            straight_copy(src_stride, dst_stride, width, src_image, dst_image, 1);
        }
    }
}

pub(crate) fn apply_icc_rgba16_impl(
    src_image: &[u16],
    src_stride: u32,
    dst_image: &mut [u16],
    dst_stride: u32,
    width: u32,
    bit_depth: u32,
    profile: &ColorProfile,
) {
    let dst_profile = ColorProfile::new_srgb();
    let transform = if bit_depth == 10 {
        profile.create_transform_10bit(
            Layout::Rgba,
            &dst_profile,
            Layout::Rgba,
            TransformOptions::default(),
        )
    } else if bit_depth == 12 {
        profile.create_transform_12bit(
            Layout::Rgba,
            &dst_profile,
            Layout::Rgba,
            TransformOptions::default(),
        )
    } else {
        profile.create_transform_16bit(
            Layout::Rgba,
            &dst_profile,
            Layout::Rgba,
            TransformOptions::default(),
        )
    };
    match transform {
        Ok(transform) => {
            for (src_row, dst_row) in src_image
                .chunks_exact(src_stride as usize)
                .zip(dst_image.chunks_exact_mut(dst_stride as usize))
            {
                let src = &src_row[..width as usize * 4];
                let dst = &mut dst_row[..width as usize * 4];
                transform.transform(src, dst).unwrap();
            }
        }
        Err(_) => {
            for (src_row, dst_row) in src_image
                .chunks_exact(src_stride as usize)
                .zip(dst_image.chunks_exact_mut(dst_stride as usize))
            {
                let src = &src_row[..width as usize * 4];
                let dst = &mut dst_row[..width as usize * 4];
                for (src, dst) in src.iter().zip(dst.iter_mut()) {
                    *dst = *src;
                }
            }
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn apply_icc_rgba16(
    src_image: *const u16,
    src_stride: u32,
    dst_image: *mut u16,
    dst_stride: u32,
    bit_depth: u32,
    width: u32,
    height: u32,
    icc_profile: *const u8,
    icc_profile_stride: u32,
) {
    unsafe {
        if bit_depth != 10 && bit_depth != 12 && bit_depth != 16 {
            let src_image = std::slice::from_raw_parts(
                src_image as *const u8,
                src_stride as usize * height as usize,
            );
            let dst_image = std::slice::from_raw_parts_mut(
                dst_image as *mut u8,
                dst_stride as usize * height as usize,
            );
            straight_copy(src_stride, dst_stride, width, src_image, dst_image, 2);
            return;
        }
        let icc_data = std::slice::from_raw_parts(icc_profile, icc_profile_stride as usize);
        match ColorProfile::new_from_slice(&icc_data) {
            Ok(icc_profile) => {
                let dst_profile = ColorProfile::new_srgb();
                let transform = if bit_depth == 10 {
                    icc_profile.create_transform_10bit(
                        Layout::Rgba,
                        &dst_profile,
                        Layout::Rgba,
                        TransformOptions::default(),
                    )
                } else if bit_depth == 12 {
                    icc_profile.create_transform_12bit(
                        Layout::Rgba,
                        &dst_profile,
                        Layout::Rgba,
                        TransformOptions::default(),
                    )
                } else {
                    icc_profile.create_transform_16bit(
                        Layout::Rgba,
                        &dst_profile,
                        Layout::Rgba,
                        TransformOptions::default(),
                    )
                };
                match transform {
                    Ok(transform) => {
                        work_on_transmuted_ptr_u16(
                            src_image,
                            src_stride,
                            width as usize,
                            height as usize,
                            true,
                            |src: &mut [u16], v_src_stride: usize| {
                                work_on_transmuted_ptr_u16(
                                    dst_image,
                                    dst_stride,
                                    width as usize,
                                    height as usize,
                                    false,
                                    |dst, v_dst_stride| {
                                        for (src_row, dst_row) in src
                                            .chunks_exact(v_src_stride)
                                            .zip(dst.chunks_exact_mut(v_dst_stride))
                                        {
                                            let src = &src_row[..width as usize * 4];
                                            let dst = &mut dst_row[..width as usize * 4];
                                            transform.transform(src, dst).unwrap();
                                        }
                                    },
                                );
                            },
                        );
                    }
                    Err(_) => {
                        let src_image = std::slice::from_raw_parts(
                            src_image as *const u8,
                            src_stride as usize * height as usize,
                        );
                        let dst_image = std::slice::from_raw_parts_mut(
                            dst_image as *mut u8,
                            dst_stride as usize * height as usize,
                        );
                        straight_copy(src_stride, dst_stride, width, src_image, dst_image, 2);
                    }
                }
            }
            Err(_) => {
                let src_image = std::slice::from_raw_parts(
                    src_image as *const u8,
                    src_stride as usize * height as usize,
                );
                let dst_image = std::slice::from_raw_parts_mut(
                    dst_image as *mut u8,
                    dst_stride as usize * height as usize,
                );
                straight_copy(src_stride, dst_stride, width, src_image, dst_image, 2);
            }
        };
    }
}

pub(crate) fn straight_copy(
    src_stride: u32,
    dst_stride: u32,
    width: u32,
    src_image: &[u8],
    dst_image: &mut [u8],
    pixel_size: usize,
) {
    for (src_row, dst_row) in src_image
        .chunks_exact(src_stride as usize)
        .zip(dst_image.chunks_exact_mut(dst_stride as usize))
    {
        let src = &src_row[..width as usize * 4 * pixel_size];
        let dst = &mut dst_row[..width as usize * 4 * pixel_size];
        for (src, dst) in src.iter().zip(dst.iter_mut()) {
            *dst = *src;
        }
    }
}

#[repr(C)]
pub struct FfiProfileData {
    pub data: *mut u8,
    pub size: usize,
    pub capacity: usize,
}

fn wrap_profile(profile: &ColorProfile) -> FfiProfileData {
    if let Ok(mut encode) = profile.encode() {
        let ptr = encode.as_mut_ptr();
        let capacity = encode.capacity();
        let size = encode.len();
        std::mem::forget(encode);
        FfiProfileData {
            data: ptr,
            size,
            capacity,
        }
    } else {
        FfiProfileData {
            data: std::ptr::null_mut(),
            size: 0,
            capacity: 0,
        }
    }
}

#[no_mangle]
pub extern "C" fn free_profile(wrapper: FfiProfileData) {
    if wrapper.data.is_null() {
        return;
    }

    unsafe {
        let _ = Vec::from_raw_parts(wrapper.data, wrapper.size, wrapper.capacity);
    }
}

#[no_mangle]
pub extern "C" fn new_dci_p3_profile() -> FfiProfileData {
    wrap_profile(&ColorProfile::new_dci_p3())
}

#[no_mangle]
pub extern "C" fn new_adobe_rgb_profile() -> FfiProfileData {
    wrap_profile(&ColorProfile::new_adobe_rgb())
}
