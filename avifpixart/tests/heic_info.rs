#![cfg(any(target_arch = "aarch64", target_arch = "arm"))]

#[path = "../src/image_info.rs"]
#[allow(dead_code)]
mod image_info;

use hpvcd::{BitDepth, ChromaFormat, CleanAperture, ColorMetadata, ImageInfo, Orientation};
use image_info::{HeicInfo, clap_rect};

fn sample_info(orientation: Orientation, clean_aperture: Option<CleanAperture>) -> ImageInfo {
    let (width, height) = match orientation {
        Orientation::Rotate90
        | Orientation::Rotate270
        | Orientation::Transpose
        | Orientation::Transverse => (932, 750),
        _ => (750, 932),
    };
    ImageInfo {
        width,
        height,
        coded_width: 750,
        coded_height: 932,
        bit_depth: BitDepth::Eight,
        chroma: ChromaFormat::Yuv420,
        color: ColorMetadata::default(),
        orientation,
        has_alpha: false,
        has_gain_map: false,
        is_grid: false,
        content_light_level: None,
        clean_aperture,
        pixel_aspect_ratio: None,
        has_exif: false,
    }
}

fn sample_clap() -> CleanAperture {
    // Exact aperture from issue #86: crop one row with a -0.5 vertical offset.
    CleanAperture {
        width_n: 750,
        width_d: 1,
        height_n: 931,
        height_d: 1,
        horiz_off_n: 0,
        horiz_off_d: 1,
        vert_off_n: -4194304,
        vert_off_d: 8388608,
    }
}

#[test]
fn clean_aperture_size_matches_decoder_crop() {
    assert_eq!(clap_rect(750, 932, &sample_clap()), Some((0, 0, 750, 931)));
    let info = sample_info(Orientation::Normal, Some(sample_clap()));
    let result = HeicInfo::from_heic_info(&info);
    assert_eq!((result.width, result.height), (750, 931));
    assert!(result.supported_image);
    assert_eq!(result.bit_depth, 8);
}

#[test]
fn aperture_is_applied_before_every_orientation() {
    for (orientation, expected) in [
        (Orientation::Normal, (750, 931)),
        (Orientation::FlipH, (750, 931)),
        (Orientation::FlipV, (750, 931)),
        (Orientation::Rotate180, (750, 931)),
        (Orientation::Rotate90, (931, 750)),
        (Orientation::Rotate270, (931, 750)),
        (Orientation::Transpose, (931, 750)),
        (Orientation::Transverse, (931, 750)),
    ] {
        let info = sample_info(orientation, Some(sample_clap()));
        let result = HeicInfo::from_heic_info(&info);
        assert_eq!((result.width, result.height), expected, "{orientation:?}");
    }
}

#[test]
fn uncropped_size_is_oriented_exactly_once() {
    for orientation in [
        Orientation::Normal,
        Orientation::FlipH,
        Orientation::FlipV,
        Orientation::Rotate180,
        Orientation::Rotate90,
        Orientation::Rotate270,
        Orientation::Transpose,
        Orientation::Transverse,
    ] {
        let info = sample_info(orientation, None);
        let result = HeicInfo::from_heic_info(&info);
        assert_eq!((result.width, result.height), (info.width, info.height));
    }
}

#[test]
fn invalid_apertures_preserve_uncropped_size_like_decoder() {
    for field in 0..6 {
        let mut clap = sample_clap();
        match field {
            0 => clap.width_d = 0,
            1 => clap.height_d = 0,
            2 => clap.horiz_off_d = 0,
            3 => clap.vert_off_d = 0,
            4 => clap.width_n = 0,
            _ => clap.height_n = 0,
        }
        assert_eq!(clap_rect(750, 932, &clap), None);
        let info = sample_info(Orientation::Rotate90, Some(clap));
        let result = HeicInfo::from_heic_info(&info);
        assert_eq!((result.width, result.height), (932, 750));
    }
}

#[test]
fn oversized_apertures_are_clamped_like_decoder_and_bit_depth_is_preserved() {
    let clap = CleanAperture {
        width_n: 1000,
        height_n: 1000,
        ..sample_clap()
    };
    assert_eq!(clap_rect(750, 932, &clap), Some((0, 0, 750, 932)));
    for bit_depth in [BitDepth::Eight, BitDepth::Ten, BitDepth::Twelve] {
        let mut info = sample_info(Orientation::Rotate270, Some(clap));
        info.bit_depth = bit_depth;
        let result = HeicInfo::from_heic_info(&info);
        assert_eq!((result.width, result.height), (932, 750));
        assert_eq!(result.bit_depth, bit_depth.bits() as u32);
    }
}
