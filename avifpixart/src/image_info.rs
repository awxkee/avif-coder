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

#[repr(C)]
#[derive(Copy, Clone)]
pub struct HeicInfo {
    pub supported_image: bool,
    pub width: u32,
    pub height: u32,
    pub bit_depth: u32,
}

impl HeicInfo {
    pub(crate) fn not_a_heic() -> HeicInfo {
        HeicInfo {
            supported_image: false,
            width: 0,
            height: 0,
            bit_depth: 0,
        }
    }
}

// HEIC metadata and decoding must crop in coded space before orientation.
#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
impl HeicInfo {
    pub(crate) fn from_heic_info(info: &hpvcd::ImageInfo) -> Self {
        let (mut width, mut height) = (info.coded_width as usize, info.coded_height as usize);
        if let Some(clap) = info.clean_aperture.as_ref()
            && let Some((_, _, crop_width, crop_height)) = clap_rect(width, height, clap)
        {
            width = crop_width;
            height = crop_height;
        }
        let (width, height) = match info.orientation {
            hpvcd::Orientation::Rotate90
            | hpvcd::Orientation::Rotate270
            | hpvcd::Orientation::Transpose
            | hpvcd::Orientation::Transverse => (height, width),
            _ => (width, height),
        };
        Self {
            width: width as u32,
            height: height as u32,
            supported_image: true,
            bit_depth: info.bit_depth.bits() as u32,
        }
    }
}

#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
pub(crate) fn clap_rect(
    width: usize,
    height: usize,
    clap: &hpvcd::CleanAperture,
) -> Option<(usize, usize, usize, usize)> {
    if clap.width_d == 0 || clap.height_d == 0 || clap.horiz_off_d == 0 || clap.vert_off_d == 0 {
        return None;
    }
    let (img_w, img_h) = (width as i64, height as i64);

    let crop_w = ((clap.width_n as i64) / (clap.width_d as i64)).min(img_w);
    let crop_h = ((clap.height_n as i64) / (clap.height_d as i64)).min(img_h);
    if crop_w <= 0 || crop_h <= 0 {
        return None;
    }

    // ISO 14496-12: offsets locate the aperture *center* relative to image center.
    //   left = horizOff + (W-1)/2 - (cropW-1)/2   (evaluated over denom 2*off_d)
    let hd = clap.horiz_off_d as i64;
    let vd = clap.vert_off_d as i64;
    let left = (2 * (clap.horiz_off_n as i64) + hd * (img_w - 1) - hd * (crop_w - 1))
        .div_euclid(2 * hd)
        .clamp(0, img_w - crop_w);
    let top = (2 * (clap.vert_off_n as i64) + vd * (img_h - 1) - vd * (crop_h - 1))
        .div_euclid(2 * vd)
        .clamp(0, img_h - crop_h);

    Some((
        left as usize,
        top as usize,
        crop_w as usize,
        crop_h as usize,
    ))
}
