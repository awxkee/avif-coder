// This harness imports the production boundary code without Android dependencies.
// Run it with rustc --test (see README.md).
#[path = "../src/box_walker.rs"]
mod box_walker;
#[path = "../src/ffi_panic.rs"]
mod ffi_panic;

use box_walker::{ImageContainer, container_recognisance};

#[test]
fn null_and_oversized_inputs_return_unknown_without_dereferencing() {
    assert_eq!(
        unsafe { container_recognisance(std::ptr::null(), 32) },
        ImageContainer::Unknown
    );
    let byte = 0u8;
    assert_eq!(
        unsafe { container_recognisance(&byte, usize::MAX) },
        ImageContainer::Unknown
    );
}

#[test]
fn valid_headers_still_identify_each_codec() {
    for (brand, expected) in [
        (*b"heic", ImageContainer::Heic),
        (*b"avif", ImageContainer::Avif),
    ] {
        let mut header = *b"\0\0\0\x10ftypheic\0\0\0\0\0\0\0\x08av1C";
        header[8..12].copy_from_slice(&brand);
        assert_eq!(
            unsafe { container_recognisance(header.as_ptr(), header.len()) },
            expected
        );
    }
    let av2 = *b"\0\0\0\x10av2C\0\0\0\0\0\0\0\0";
    assert_eq!(
        unsafe { container_recognisance(av2.as_ptr(), av2.len()) },
        ImageContainer::Av2
    );
}

#[test]
fn truncated_and_malformed_headers_return_normally_through_c_abi() {
    for bytes in [
        *b"\0\0\0\x10ftypheic\0\0\0\0",
        *b"\0\0\0\x01ftyp\xff\xff\xff\xff\xff\xff\xff\xff",
        *b"\0\0\0\x02ftypmif1\0\0\0\0",
    ] {
        for length in 0..bytes.len() {
            assert_eq!(
                unsafe { container_recognisance(bytes.as_ptr(), length) },
                ImageContainer::Unknown
            );
        }
    }
}
