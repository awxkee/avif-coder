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
use jni::objects::JObject;
use jni::sys::jobject;
use jni::{Env, JValue, jni_sig, jni_str};
use num_traits::AsPrimitive;
use std::fmt::Debug;
use std::ops::{AddAssign, BitXor};
use std::slice;
use std::sync::OnceLock;

#[inline]
pub(crate) fn transmute_const_ptr16(
    ptr: *const u16,
    stride: usize,
    width: usize,
    height: usize,
    chans: usize,
) -> (std::borrow::Cow<'static, [u16]>, usize) {
    let slice = unsafe { slice::from_raw_parts(ptr as *const u8, stride * height) };
    if let Ok(casted) = bytemuck::try_cast_slice(slice) {
        return (std::borrow::Cow::Borrowed(casted), stride / 2);
    }
    let mut new_store = vec![0u16; width * height * chans];
    let new_stride = width * chans;
    for (dst, src) in new_store
        .chunks_exact_mut(new_stride)
        .zip(slice.chunks_exact(stride))
    {
        for (dst, src) in dst
            .iter_mut()
            .zip(src.as_chunks::<2>().0.iter())
            .take(width * chans)
        {
            *dst = u16::from_ne_bytes([src[0], src[1]]);
        }
    }
    (std::borrow::Cow::Owned(new_store), new_stride)
}

#[derive(Debug)]
pub(crate) enum SliceStoreMut<'a, T: Copy + Debug> {
    Borrowed(&'a mut [T]),
    Owned(Vec<T>),
}

impl<T: Copy + Debug> SliceStoreMut<'_, T> {
    #[allow(clippy::should_implement_trait)]
    pub fn borrow(&self) -> &[T] {
        match self {
            Self::Borrowed(p_ref) => p_ref,
            Self::Owned(vec) => vec,
        }
    }

    #[allow(clippy::should_implement_trait)]
    pub fn borrow_mut(&mut self) -> &mut [T] {
        match self {
            Self::Borrowed(p_ref) => p_ref,
            Self::Owned(vec) => vec,
        }
    }
}

macro_rules! try_vec {
    () => {
        Vec::new()
    };
    ($elem:expr; $n:expr) => {{
        let mut v = Vec::new();
        v.try_reserve_exact($n)
            .map_err(|_| WeaverError::FailedToAllocateMemory($n as u64))?;
        v.resize($n, $elem);
        v
    }};
}

use crate::WeaverPreferredColorConfig;
pub(crate) use try_vec;

pub(crate) const MIN_OS_F16: i32 = 26;
pub(crate) const MIN_OS_AR30: i32 = 33;

pub(crate) struct PackedImageBuffer {
    pub(crate) data: Vec<u8>,
    pub(crate) width: usize,
    pub(crate) height: usize,
    pub(crate) format: WeaverPreferredColorConfig,
}

pub(crate) enum PackedImageTransfer {
    Image(PackedImageBuffer),
    HardwareBuffer(jobject),
}

static SDK_VERSION: OnceLock<i32> = OnceLock::new();

pub(crate) fn android_os_version() -> i32 {
    *SDK_VERSION.get_or_init(|| {
        let key = b"ro.build.version.sdk\0";
        let mut buf = [0u8; 92]; // PROP_VALUE_MAX

        unsafe extern "C" {
            fn __system_property_get(
                name: *const std::ffi::c_char,
                value: *mut std::ffi::c_char,
            ) -> std::ffi::c_int;
        }

        let len = unsafe { __system_property_get(key.as_ptr().cast(), buf.as_mut_ptr().cast()) };

        if len <= 0 {
            return 0;
        }

        std::ffi::CStr::from_bytes_until_nul(&buf)
            .ok()
            .and_then(|s| s.to_str().ok())
            .and_then(|s| s.parse().ok())
            .unwrap_or(0)
    })
}

macro_rules! dbg_log {
    ($level:ident, $($arg:tt)*) => {
        #[cfg(feature = "logging")]
        log::$level!(target: "avifweaver", $($arg)*);
    };
}
pub(crate) use dbg_log;

#[cfg(feature = "logging")]
static LOGGER: OnceLock<()> = OnceLock::new();

pub(crate) fn init_logging() {
    #[cfg(feature = "logging")]
    LOGGER.get_or_init(|| {
        android_logger::init_once(
            android_logger::Config::default()
                .with_max_level(log::LevelFilter::Debug)
                .with_tag("avifweaver"),
        );
    });
}

pub(crate) fn optional_bytebuffer_to_vec(
    env: &mut Env,
    obj: jobject,
) -> Result<Option<Vec<u8>>, jni::errors::Error> {
    if obj.is_null() {
        return Ok(None);
    }

    let buf = unsafe { JObject::from_raw(env, obj) };

    let remaining = env
        .call_method(&buf, jni_str!("remaining"), jni_sig!("()I"), &[])?
        .i()? as usize;

    if remaining == 0 {
        return Ok(Some(Vec::new()));
    }

    let dup = env
        .call_method(
            &buf,
            jni_str!("duplicate"),
            jni_sig!("()Ljava/nio/ByteBuffer;"),
            &[],
        )?
        .l()?;

    let arr = env.new_byte_array(remaining)?;
    env.call_method(
        &dup,
        jni_str!("get"),
        jni_sig!("([B)Ljava/nio/ByteBuffer;"),
        &[JValue::Object(&arr)],
    )?;

    Ok(Some(env.convert_byte_array(&arr)?))
}

pub(crate) fn has_non_constant_alpha<
    V: Copy + PartialEq + BitXor<V, Output = V> + 'static + AsPrimitive<J> + 'static,
    J: Copy + AddAssign + Default + 'static + Eq + Ord,
    const ALPHA_CHANNEL_INDEX: usize,
    const CHANNELS: usize,
>(
    store: &[V],
    width: usize,
) -> bool
where
    i32: AsPrimitive<V>,
    u32: AsPrimitive<V> + AsPrimitive<J>,
{
    assert!(ALPHA_CHANNEL_INDEX < CHANNELS);
    assert!(CHANNELS > 0 && CHANNELS <= 4);
    if store.is_empty() {
        return false;
    }
    let first = store[0];
    let mut row_sums: J = 0u32.as_();
    for row in store.chunks_exact(width * CHANNELS) {
        for color in row.as_chunks::<CHANNELS>().0.iter() {
            row_sums += color[ALPHA_CHANNEL_INDEX].bitxor(first).as_();
        }
        if row_sums != 0.as_() {
            return true;
        }
    }
    let zeros = 0.as_();
    row_sums.ne(&zeros)
}
