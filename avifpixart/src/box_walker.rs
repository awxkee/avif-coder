/*
 * Copyright (c) Radzivon Bartoshyk 2026/6. All rights reserved.
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

#[inline]
fn brand_matches(brand: &[u8], table: &[&[u8; 4]]) -> bool {
    let Ok(b) = brand.try_into() else {
        return false;
    };
    table.contains(&b)
}

fn ftyp_box_size(bytes: &[u8]) -> Option<usize> {
    let size = u32::from_be_bytes(bytes[0..4].try_into().ok()?) as usize;
    if size >= 16 && size <= bytes.len() {
        Some(size)
    } else {
        None
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ImageContainer {
    Unknown = 0,
    Heic = 1, // HEVC codec (hvcC box)
    Avif = 2, // AV1  codec (av1C box)
    Av2 = 3,  // AV2  codec (av2C box, future)
    Vvc = 4,  // VVC/H.266 codec (vvcC box)
}

static HEVC_MAJOR_BRANDS: &[&[u8; 4]] = &[b"heic", b"heix", b"hevc", b"hevx", b"MiHE"];
static AV1_MAJOR_BRANDS: &[&[u8; 4]] = &[b"avif", b"avis", b"MA1A", b"MA1B"];
static UMBRELLA_BRANDS: &[&[u8; 4]] = &[b"mif1", b"msf1", b"miaf"];

/// A single parsed box header — points into the original slice.
struct Box<'a> {
    kind: [u8; 4],
    payload: &'a [u8],
}

/// Parse the next box at `data[0..]`.
/// Returns `(box, bytes_consumed)` or `None` on a malformed box.
fn parse_box(data: &[u8]) -> Option<(Box<'_>, usize)> {
    if data.len() < 8 {
        return None;
    }
    let size_field = u32::from_be_bytes(data[0..4].try_into().unwrap());
    let kind: [u8; 4] = data[4..8].try_into().unwrap();

    let (header, total) = match size_field {
        0 => (8, data.len()), // box extends to end of file
        1 => {
            // 64-bit largesize
            if data.len() < 16 {
                return None;
            }
            let large = u64::from_be_bytes(data[8..16].try_into().unwrap()) as usize;
            if large < 16 || large > data.len() {
                return None;
            }
            (16, large)
        }
        s if (s as usize) < 8 => return None,
        s => (8, s as usize),
    };

    if total > data.len() {
        return None;
    }

    Some((
        Box {
            kind,
            payload: &data[header..total],
        },
        total,
    ))
}

/// Recursively walk ISOBMFF boxes looking for a codec-config box.
fn walk_boxes(data: &[u8], depth: u8) -> ImageContainer {
    if depth > 8 {
        return ImageContainer::Unknown;
    }

    let mut pos = 0;
    while pos < data.len() {
        let Some((b, consumed)) = parse_box(&data[pos..]) else {
            break;
        };

        match &b.kind {
            b"hvcC" => return ImageContainer::Heic,
            b"av1C" => return ImageContainer::Avif,
            b"av2C" => return ImageContainer::Av2,
            b"vvcC" => return ImageContainer::Vvc,

            b"infe" if b.payload.len() >= 16 => {
                let item_type = &b.payload[12..16];
                match item_type {
                    b"hvc1" | b"hev1" => return ImageContainer::Heic,
                    b"av01" => return ImageContainer::Avif,
                    // VVC image item: `vvc1` (parameter sets out-of-band) or
                    // `vvi1` (parameter sets may be carried in-band).
                    b"vvc1" | b"vvi1" => return ImageContainer::Vvc,
                    _ => {}
                }
            }

            b"moov" | b"trak" | b"mdia" | b"minf" | b"stbl" | b"moof" | b"traf" | b"iprp"
            | b"ipco" | b"iinf" => {
                let r = walk_boxes(b.payload, depth + 1);
                if r != ImageContainer::Unknown {
                    return r;
                }
            }

            b"meta" if b.payload.len() > 4 => {
                let r = walk_boxes(&b.payload[4..], depth + 1);
                if r != ImageContainer::Unknown {
                    return r;
                }
            }

            _ => {}
        }

        pos += consumed;
    }

    ImageContainer::Unknown
}

fn detect_container(bytes: &[u8]) -> ImageContainer {
    if bytes.len() < 16 {
        return ImageContainer::Unknown;
    }

    if &bytes[4..8] == b"ftyp" {
        let major: &[u8; 4] = bytes[8..12].try_into().unwrap();

        if brand_matches(major, HEVC_MAJOR_BRANDS) {
            return ImageContainer::Heic;
        }
        if brand_matches(major, AV1_MAJOR_BRANDS) {
            return ImageContainer::Avif;
        }

        if brand_matches(major, UMBRELLA_BRANDS) {
            let box_end = ftyp_box_size(bytes).unwrap_or(bytes.len().min(512));
            for chunk in bytes[16..box_end].chunks_exact(4) {
                if brand_matches(chunk, HEVC_MAJOR_BRANDS) {
                    return ImageContainer::Heic;
                }
                if brand_matches(chunk, AV1_MAJOR_BRANDS) {
                    return ImageContainer::Avif;
                }
            }
        }
    }

    walk_boxes(bytes, 0)
}

fn detect_image_container(data: *const u8, len: usize) -> ImageContainer {
    if len < 16 {
        return ImageContainer::Unknown;
    }
    detect_container(unsafe { slice::from_raw_parts(data, len) })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn is_heic_image(data: *const u8, len: usize) -> bool {
    detect_image_container(data, len) == ImageContainer::Heic
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn is_avif_image(data: *const u8, len: usize) -> bool {
    detect_image_container(data, len) == ImageContainer::Avif
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn is_vvc_image(data: *const u8, len: usize) -> bool {
    detect_image_container(data, len) == ImageContainer::Vvc
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn is_av2_image(data: *const u8, len: usize) -> bool {
    detect_image_container(data, len) == ImageContainer::Avif
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn container_recognisance(data: *const u8, len: usize) -> ImageContainer {
    detect_image_container(data, len)
}
