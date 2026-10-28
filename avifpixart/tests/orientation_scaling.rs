#![feature(f16)]
#![cfg(any(target_arch = "aarch64", target_arch = "arm"))]

// Exercise the Android pixel-processing code on an ARM host without JNI.
#[path = "../src/orientation.rs"]
mod orientation;
#[path = "../src/scaling.rs"]
#[allow(dead_code)]
mod scaling;

use std::borrow::Cow;

fn check_orientations<T: Copy + Default + std::fmt::Debug + PartialEq>(
    source: &[T],
    orient: impl Fn(&[T], usize, usize, usize) -> (Vec<T>, usize, usize),
) {
    // A non-square image makes swapped dimensions and row boundaries observable.
    let cases: [(usize, usize, [usize; 6]); 8] = [
        (3, 2, [0, 1, 2, 3, 4, 5]), // normal
        (3, 2, [2, 1, 0, 5, 4, 3]), // horizontal mirror
        (3, 2, [3, 4, 5, 0, 1, 2]), // vertical mirror
        (3, 2, [5, 4, 3, 2, 1, 0]), // 180 degrees
        (2, 3, [3, 0, 4, 1, 5, 2]), // 90 degrees clockwise
        (2, 3, [2, 5, 1, 4, 0, 3]), // 270 degrees clockwise
        (2, 3, [0, 3, 1, 4, 2, 5]), // transpose
        (2, 3, [5, 2, 4, 1, 3, 0]), // transverse
    ];
    for (index, (expected_width, expected_height, order)) in cases.iter().enumerate() {
        let (output, width, height) = orient(source, 3, 2, index);
        assert_eq!((width, height), (*expected_width, *expected_height));
        assert_eq!(output.len(), width * height * 4, "orientation {index}");
        let expected: Vec<T> = order
            .iter()
            .flat_map(|&pixel| source[pixel * 4..pixel * 4 + 4].iter().copied())
            .collect();
        assert_eq!(
            output, expected,
            "orientation {index} must preserve RGBA pixels"
        );
    }
}

fn heic_orientation(index: usize) -> hpvcd::Orientation {
    use hpvcd::Orientation::*;
    [
        Normal, FlipH, FlipV, Rotate180, Rotate90, Rotate270, Transpose, Transverse,
    ][index]
}

fn av2_orientation(index: usize) -> tealdust::Orientation {
    use tealdust::Orientation::*;
    [
        Normal, FlipH, FlipV, Rotate180, Rotate90, Rotate270, Transpose, Transverse,
    ][index]
}

#[test]
fn heic_orientations_preserve_rgba8_and_rgba16() {
    // Each channel, including alpha, has its own value. u16 uses samples above 255.
    let rgba8: Vec<u8> = (0..24).collect();
    let rgba16: Vec<u16> = (512..536).collect();
    check_orientations(&rgba8, |px, w, h, i| {
        orientation::apply_orientation(px, w, h, heic_orientation(i))
    });
    check_orientations(&rgba16, |px, w, h, i| {
        orientation::apply_orientation(px, w, h, heic_orientation(i))
    });
}

#[test]
fn av2_orientations_preserve_rgba8_and_rgba16() {
    let rgba8: Vec<u8> = (0..24).collect();
    let rgba16: Vec<u16> = (512..536).collect();
    check_orientations(&rgba8, |px, w, h, i| {
        orientation::apply_orientation_av2(px, w, h, av2_orientation(i))
    });
    check_orientations(&rgba16, |px, w, h, i| {
        orientation::apply_orientation_av2(px, w, h, av2_orientation(i))
    });
}

#[test]
fn oriented_rgba_buffers_can_be_scaled_in_every_mode() {
    use scaling::WeaveScaleMode::*;
    // A rotated 8x6 image needs source rows beyond the truncated RGB buffer
    // when center-cropped to a square, reproducing the reported crop failure.
    let rgba8: Vec<u8> = (0..192).collect();
    let rgba16: Vec<u16> = (512..704).collect();
    for index in 0..8 {
        for av2 in [false, true] {
            for mode in [ScaleToFill, ScaleToFit, JustResize] {
                let (px8, w, h) = if av2 {
                    orientation::apply_orientation_av2(&rgba8, 8, 6, av2_orientation(index))
                } else {
                    orientation::apply_orientation(&rgba8, 8, 6, heic_orientation(index))
                };
                let scaled8 = scaling::internal_scale_u8(
                    Cow::Owned(px8),
                    (w * 4) as u32,
                    w as u32,
                    h as u32,
                    2,
                    2,
                    false,
                    mode,
                )
                .expect("oriented RGBA8 must have enough data for scaling/cropping");
                assert_eq!(
                    scaled8.buffer.borrow().len(),
                    scaled8.width * scaled8.height * 4
                );

                let (px16, w, h) = if av2 {
                    orientation::apply_orientation_av2(&rgba16, 8, 6, av2_orientation(index))
                } else {
                    orientation::apply_orientation(&rgba16, 8, 6, heic_orientation(index))
                };
                let scaled16 = scaling::internal_scale_u16(
                    Cow::Owned(px16),
                    w * 4,
                    w as u32,
                    h as u32,
                    2,
                    2,
                    10,
                    false,
                    mode,
                )
                .expect("oriented RGBA16 must have enough data for scaling/cropping");
                assert_eq!(
                    scaled16.buffer.borrow().len(),
                    scaled16.width * scaled16.height * 4
                );
            }
        }
    }
}
