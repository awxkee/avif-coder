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
use crate::heic_decode::WeaverError;
use crate::support::try_vec;
use core::f16;
use yuv::{
    BufferStoreMut, Rgb30ByteOrder, convert_rgba_f16_to_rgba16, convert_rgba_to_f16,
    convert_rgba16_to_f16, rgba8_to_ar30, rgba10_to_ar30, rgba12_to_ar30,
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
        if (rgba as usize).is_multiple_of(2) && rgba_stride.is_multiple_of(2) {
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
        let dst_slice: &mut [u8] = unsafe {
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
        if (rgba as usize).is_multiple_of(2) && rgba_stride.is_multiple_of(2) {
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

#[unsafe(no_mangle)]
pub unsafe extern "C" fn weave_cvt_rgba8_to_rgba_f16(
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

#[unsafe(no_mangle)]
pub unsafe extern "C" fn weave_premultiply_rgba_f16(
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
fn premultiply_default_aarch64_fp_16(x: &mut [f16], dst_stride: usize) {
    for lane in x.chunks_exact_mut(dst_stride) {
        for chunk in lane.chunks_exact_mut(4) {
            let a = chunk[3];
            chunk[0] *= a;
            chunk[1] *= a;
            chunk[2] *= a;
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn weave_cvt_rgba8_to_ar30(
    rgba8: *const u8,
    rgba8_stride: u32,
    ar30: *mut u8,
    ar30_stride: u32,
    width: u32,
    height: u32,
) {
    let src_slice =
        unsafe { std::slice::from_raw_parts(rgba8, rgba8_stride as usize * height as usize) };
    let dst_slice =
        unsafe { std::slice::from_raw_parts_mut(ar30, ar30_stride as usize * height as usize) };
    rgba8_to_ar30(
        dst_slice,
        ar30_stride,
        Rgb30ByteOrder::Host,
        src_slice,
        rgba8_stride,
        width,
        height,
    )
    .unwrap();
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn weave_cvt_rgba16_to_ar30(
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
            let dst_slice = unsafe {
                std::slice::from_raw_parts_mut(ar30, ar30_stride as usize * height as usize)
            };
            let handle = if bit_depth == 10 {
                rgba10_to_ar30
            } else {
                rgba12_to_ar30
            };
            handle(
                dst_slice,
                ar30_stride,
                Rgb30ByteOrder::Host,
                src,
                src_stride as u32,
                width,
                height,
            )
            .unwrap();
        },
    );
}

#[unsafe(no_mangle)]
pub extern "C" fn weave_cvt_rgba16_to_rgba_f16(
    rgba16: *const u16,
    rgba16_stride: u32,
    bit_depth: usize,
    rgba_f16: *mut u16,
    rgba_f16_stride: u32,
    width: u32,
    height: u32,
) {
    if rgba16 == rgba_f16 {
        let mut new_src = vec![0; width as usize * height as usize * 4];
        let src_slice = unsafe {
            std::slice::from_raw_parts(rgba16 as *mut u8, rgba16_stride as usize * height as usize)
        };
        for (dst, src) in new_src
            .chunks_exact_mut(width as usize * 4)
            .zip(src_slice.chunks_exact(rgba16_stride as usize))
        {
            let src = &src[0..width as usize * 4 * 2];
            let dst = &mut dst[0..width as usize * 4];
            for (dst, src) in dst.iter_mut().zip(src.as_chunks::<2>().0.iter()) {
                *dst = u16::from_ne_bytes([src[0], src[1]]);
            }
        }

        work_on_transmuted_ptr_f16(
            rgba_f16,
            rgba_f16_stride,
            width as usize,
            height as usize,
            false,
            |dst: &mut [f16], dst_stride: usize| {
                convert_rgba16_to_f16(
                    &new_src,
                    width as usize * 4,
                    dst,
                    dst_stride,
                    bit_depth,
                    width as usize,
                    height as usize,
                )
                .unwrap();
            },
        );
    } else {
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
}

pub(crate) fn pack_10_or_12_to_ar30(
    bytes: &[u16],
    width: usize,
    height: usize,
    bit_depth: usize,
) -> Result<Vec<u8>, anyhow::Error> {
    assert!(bit_depth == 10 || bit_depth == 12);
    let mut ar30 = try_vec![0u8; width * height * 4];
    match bit_depth {
        10 => rgba10_to_ar30(
            &mut ar30,
            width as u32 * 4,
            Rgb30ByteOrder::Host,
            bytes,
            width as u32 * 4,
            width as u32,
            height as u32,
        )
        .map_err(|x| anyhow::anyhow!(x))?,
        12 => rgba12_to_ar30(
            &mut ar30,
            width as u32 * 4,
            Rgb30ByteOrder::Host,
            bytes,
            width as u32 * 4,
            width as u32,
            height as u32,
        )
        .map_err(|x| anyhow::anyhow!(x))?,
        _ => unreachable!(),
    };
    Ok(ar30
        .as_chunks::<2>()
        .0
        .iter()
        .flat_map(|x| [x[0], x[1]])
        .collect::<Vec<_>>())
}

pub(crate) fn pack_8_to_ar30(
    bytes: &[u8],
    width: usize,
    height: usize,
) -> Result<Vec<u8>, anyhow::Error> {
    let mut ar30 = try_vec![0u8; width * height * 4];
    rgba8_to_ar30(
        &mut ar30,
        width as u32 * 4,
        Rgb30ByteOrder::Host,
        bytes,
        width as u32 * 4,
        width as u32,
        height as u32,
    )
    .map_err(|x| anyhow::anyhow!(x))?;
    Ok(ar30
        .as_chunks::<2>()
        .0
        .iter()
        .flat_map(|x| [x[0], x[1]])
        .collect::<Vec<_>>())
}

pub(crate) fn pack_8_to_f16(
    bytes: &[u8],
    width: usize,
    height: usize,
) -> Result<Vec<u8>, anyhow::Error> {
    let mut ar30 = try_vec![0_f16; width * height * 4];
    convert_rgba_to_f16(bytes, width * 4, &mut ar30, width * 4, width, height)
        .map_err(|x| anyhow::anyhow!(x))?;
    Ok(ar30
        .iter()
        .flat_map(|a| {
            let x = a.to_ne_bytes();
            [x[0], x[1]]
        })
        .collect::<Vec<_>>())
}

pub(crate) fn pack_10_or_12_to_f16(
    bytes: &[u16],
    width: usize,
    height: usize,
    bit_depth: usize,
) -> Result<Vec<u8>, anyhow::Error> {
    assert!(bit_depth == 10 || bit_depth == 12);
    let mut ar30 = try_vec![0_f16; width * height * 4];
    convert_rgba16_to_f16(
        bytes,
        width * 4,
        &mut ar30,
        width * 4,
        bit_depth,
        width,
        height,
    )
    .map_err(|x| anyhow::anyhow!(x))?;
    Ok(ar30
        .iter()
        .flat_map(|a| {
            let x = a.to_ne_bytes();
            [x[0], x[1]]
        })
        .collect::<Vec<_>>())
}

/// Converts 8-bit RGBA bytes → packed RGB565, 2 bytes per pixel (native-endian).
/// Alpha is discarded — RGB565 carries no alpha channel.
pub(crate) fn pack_8_to_565(
    bytes: &[u8],
    width: usize,
    height: usize,
) -> Result<Vec<u8>, anyhow::Error> {
    let pixel_count = width * height;
    if bytes.len() < pixel_count * 4 {
        return Err(anyhow::anyhow!(
            "8-bit RGBA buffer too small: need {} bytes, got {}",
            pixel_count * 4,
            bytes.len()
        ));
    }

    let mut out = try_vec![0_u8; pixel_count * 2];

    for (dst, src) in out
        .as_chunks_mut::<2>()
        .0
        .iter_mut()
        .zip(bytes.as_chunks::<4>().0.iter())
    {
        let r = src[0] as u16;
        let g = src[1] as u16;
        let b = src[2] as u16;

        let r5 = (r >> 3).min(0x1F);
        let g6 = (g >> 2).min(0x3F);
        let b5 = (b >> 3).min(0x1F);

        let px: u16 = (r5 << 11) | (g6 << 5) | b5;
        let encoded = px.to_ne_bytes();
        dst[0] = encoded[0];
        dst[1] = encoded[1];
    }

    Ok(out)
}

pub(crate) fn pack_10_or_12_to_565(
    bytes: &[u16],
    width: usize,
    height: usize,
    bit_depth: usize,
) -> Result<Vec<u8>, anyhow::Error> {
    assert!(bit_depth == 10 || bit_depth == 12);

    let pixel_count = width * height;
    if bytes.len() < pixel_count * 4 {
        return Err(anyhow::anyhow!(
            "{}-bit RGBA buffer too small: need {} u16 values, got {}",
            bit_depth,
            pixel_count * 4,
            bytes.len()
        ));
    }

    let shift_r = bit_depth - 5;
    let shift_g = bit_depth - 6;
    let shift_b = bit_depth - 5;

    let mut out = try_vec![0_u8; pixel_count * 2];

    for (dst, src) in out
        .as_chunks_mut::<2>()
        .0
        .iter_mut()
        .zip(bytes.as_chunks::<4>().0.iter())
    {
        let r = src[0] as u32;
        let g = src[1] as u32;
        let b = src[2] as u32;

        let r5 = ((r >> shift_r) as u16).min(0x1F); // 5 bits → max 31
        let g6 = ((g >> shift_g) as u16).min(0x3F); // 6 bits → max 63
        let b5 = ((b >> shift_b) as u16).min(0x1F); // 5 bits → max 31

        let px: u16 = (r5 << 11) | (g6 << 5) | b5;
        let encoded = px.to_ne_bytes();
        dst[0] = encoded[0];
        dst[1] = encoded[1];
    }

    Ok(out)
}

#[inline(always)]
pub fn rgb565_to_rgba8888(px: u16) -> [u8; 4] {
    let r5 = ((px >> 11) & 0x1F) as u8;
    let g6 = ((px >> 5) & 0x3F) as u8;
    let b5 = (px & 0x1F) as u8;

    [
        (r5 << 3) | (r5 >> 2), // 5→8: replicate top bits into the gap
        (g6 << 2) | (g6 >> 4), // 6→8
        (b5 << 3) | (b5 >> 2), // 5→8
        0xFF,                  // opaque — 565 has no alpha channel
    ]
}

pub(crate) fn rgb565_bytes_to_rgba8888(src: &[u8]) -> Vec<u8> {
    assert_eq!(src.len() % 2, 0, "rgb565 buffer must have even length");
    let pixels: &[[u8; 2]] = src.as_chunks().0;
    let mut dst = Vec::with_capacity(pixels.len() * 4);
    dst.extend(
        pixels
            .iter()
            .map(|&b| u16::from_le_bytes(b))
            .flat_map(rgb565_to_rgba8888),
    );
    dst
}

#[inline(always)]
pub fn ar30_to_rgba10_pixel(px: u32) -> [u16; 4] {
    let r10 = (px & 0x3FF) as u16;
    let g10 = ((px >> 10) & 0x3FF) as u16;
    let b10 = ((px >> 20) & 0x3FF) as u16;
    let a2 = ((px >> 30) & 0x003) as u16;

    // 2→10 bit expansion via replication: identical to (a2 << 8)|(a2 << 6)|…
    let a10 = a2 * 341;

    [r10, g10, b10, a10]
}

/// Convert a slice of AR30 `u32` words into a tightly-packed RGBA u16 buffer
/// (`width × height × 4` elements, each in `0..=1023`).
pub(crate) fn ar30_to_rgba10(src: &[u32]) -> Vec<u16> {
    let mut dst = Vec::with_capacity(src.len() * 4);
    dst.extend(src.iter().flat_map(|&px| ar30_to_rgba10_pixel(px)));
    dst
}

/// Convert a raw `&[u8]` (4 bytes per pixel, little-endian u32) produced by
/// locking an Android `RGBA_1010102` Bitmap, into a RGBA u16 10-bit buffer.
pub(crate) fn ar30_bytes_to_rgba10(src: &[u8]) -> Vec<u16> {
    assert_eq!(
        src.len() % 4,
        0,
        "AR30 buffer length must be a multiple of 4"
    );
    src.as_chunks::<4>()
        .0
        .iter()
        .flat_map(|b| ar30_to_rgba10_pixel(u32::from_le_bytes(*b)))
        .collect()
}

/// In-place variant — writes into a pre-allocated `&mut [u16]`.
/// Panics if `dst.len() < src.len() * 4`.
pub(crate) fn ar30_to_rgba10_into(src: &[u32], dst: &mut [u16]) {
    assert!(
        dst.len() >= src.len() * 4,
        "dst too small: need {} u16 elements, got {}",
        src.len() * 4,
        dst.len(),
    );
    for (&px, chunk) in src.iter().zip(dst.chunks_exact_mut(4)) {
        let [r, g, b, a] = ar30_to_rgba10_pixel(px);
        chunk[0] = r;
        chunk[1] = g;
        chunk[2] = b;
        chunk[3] = a;
    }
}

#[inline(always)]
pub(crate) fn rgba10_to_ar30_pixel(rgba: [u16; 4]) -> u32 {
    let [r, g, b, a] = rgba;
    let a2 = (a >> 8) as u32; // 10→2: keep top 2 bits
    ((a2 & 0x3) << 30)
        | (((b & 0x3FF) as u32) << 20)
        | (((g & 0x3FF) as u32) << 10)
        | ((r & 0x3FF) as u32)
}

pub(crate) fn f16_bytes_to_rgba10(
    src: &[u8],
    width: usize,
    height: usize,
) -> Result<Vec<u16>, anyhow::Error> {
    assert_eq!(
        src.len() % 4,
        0,
        "F16 buffer length must be a multiple of 4"
    );
    let src_f16 = src
        .as_chunks::<2>()
        .0
        .iter()
        .map(|b| f16::from_ne_bytes(*b))
        .collect::<Vec<f16>>();
    let mut dst_u16 = try_vec![0u16; width * height * 4];
    convert_rgba_f16_to_rgba16(
        &src_f16,
        width * 4,
        &mut dst_u16,
        width * 4,
        10,
        width,
        height,
    )
    .map_err(|x| anyhow::anyhow!(x))?;
    Ok(dst_u16)
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::f16;
    #[test]
    fn test_u16_f16() {
        let width = 16usize;
        let height = 16usize;
        let origin = vec![512; width * height * 4];
        let mut dst: Vec<f16> = vec![0.; width * height * 4];
        weave_cvt_rgba16_to_rgba_f16(
            origin.as_ptr(),
            width as u32 * 4 * 2,
            10,
            dst.as_mut_ptr() as *mut _,
            width as u32 * 4 * 2,
            width as u32,
            height as u32,
        );
        let v_src = origin
            .iter()
            .map(|&x| (x as f32 / 1023f32) as f16)
            .collect::<Vec<f16>>();
        assert_eq!(dst, v_src);

        let mut origin = vec![512; width * height * 4];
        weave_cvt_rgba16_to_rgba_f16(
            origin.as_ptr(),
            width as u32 * 4 * 2,
            10,
            origin.as_mut_ptr() as *mut _,
            width as u32 * 4 * 2,
            width as u32,
            height as u32,
        );
        let v_src = origin
            .iter()
            .map(|&x| f16::from_bits(x))
            .collect::<Vec<f16>>();
        assert_eq!(dst, v_src);
    }

    #[test]
    fn test_u16_f16_12() {
        let width = 16usize;
        let height = 16usize;
        let origin = vec![4095; width * height * 4];
        let mut dst: Vec<f16> = vec![0.; width * height * 4];
        weave_cvt_rgba16_to_rgba_f16(
            origin.as_ptr(),
            width as u32 * 4 * 2,
            12,
            dst.as_mut_ptr() as *mut _,
            width as u32 * 4 * 2,
            width as u32,
            height as u32,
        );
        let v_src = origin
            .iter()
            .map(|&x| (x as f32 / 4095f32) as f16)
            .collect::<Vec<f16>>();
        assert_eq!(dst, v_src);

        let mut origin = vec![512; width * height * 4];
        weave_cvt_rgba16_to_rgba_f16(
            origin.as_ptr(),
            width as u32 * 4 * 2,
            12,
            origin.as_mut_ptr() as *mut _,
            width as u32 * 4 * 2,
            width as u32,
            height as u32,
        );
        let v_src = origin
            .iter()
            .map(|&x| f16::from_bits(x))
            .collect::<Vec<f16>>();
        assert_eq!(dst, v_src);
    }

    #[test]
    fn alpha_2bit_expansion() {
        assert_eq!(ar30_to_rgba10_pixel(0 << 30)[3], 0);
        assert_eq!(ar30_to_rgba10_pixel(1 << 30)[3], 341);
        assert_eq!(ar30_to_rgba10_pixel(2 << 30)[3], 682);
        assert_eq!(ar30_to_rgba10_pixel(3 << 30)[3], 1023);
    }

    #[test]
    fn rgb_channel_isolation() {
        // R = 1023, G = 0, B = 0, A = 0
        let px = 0x3FF_u32;
        let [r, g, b, _] = ar30_to_rgba10_pixel(px);
        assert_eq!((r, g, b), (1023, 0, 0));

        // G = 512
        let px = 512_u32 << 10;
        let [r, g, b, _] = ar30_to_rgba10_pixel(px);
        assert_eq!((r, g, b), (0, 512, 0));

        // B = 1
        let px = 1_u32 << 20;
        let [r, g, b, _] = ar30_to_rgba10_pixel(px);
        assert_eq!((r, g, b), (0, 0, 1));
    }

    #[test]
    fn roundtrip_opaque() {
        for r in [0u16, 128, 512, 1023] {
            for b in [0u16, 255, 1023] {
                let original = [r, 600, b, 1023];
                let packed = rgba10_to_ar30_pixel(original);
                let unpacked = ar30_to_rgba10_pixel(packed);
                // RGB survives exactly; alpha loses bottom 8 bits (2-bit storage)
                assert_eq!(unpacked[0], original[0], "R mismatch");
                assert_eq!(unpacked[1], original[1], "G mismatch");
                assert_eq!(unpacked[2], original[2], "B mismatch");
            }
        }
    }

    #[test]
    fn byte_slice_entry_point() {
        let px: u32 = (3 << 30) | (1023 << 20) | (512 << 10) | 0;
        let bytes = px.to_le_bytes();
        let result = ar30_bytes_to_rgba10(&bytes);
        assert_eq!(result, &[0, 512, 1023, 1023]);
    }
}
