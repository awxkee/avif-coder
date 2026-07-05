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
#![allow(unused)]

use jni::Env;
use jni::sys::{JNIEnv, jobject};
use ndk_sys::{
    AHardwareBuffer, AHardwareBuffer_Desc, AHardwareBuffer_Format, AHardwareBuffer_UsageFlags,
    ARect,
};
use std::marker::PhantomData;
use std::sync::OnceLock;

pub type AllocateFn = unsafe extern "C" fn(
    desc: *const AHardwareBuffer_Desc,
    out_buffer: *mut *mut AHardwareBuffer,
) -> i32;

pub type IsSupportedFn = unsafe extern "C" fn(desc: *const AHardwareBuffer_Desc) -> i32;

pub type ReleaseFn = unsafe extern "C" fn(buffer: *mut AHardwareBuffer);

pub type UnlockFn = unsafe extern "C" fn(buffer: *mut AHardwareBuffer, fence: *mut i32) -> i32;

pub type LockFn = unsafe extern "C" fn(
    buffer: *mut AHardwareBuffer,
    usage: u64,
    fence: i32,
    rect: *const ARect,
    out_virtual_address: *mut *mut std::ffi::c_void,
) -> i32;

pub type DescribeFn =
    unsafe extern "C" fn(buffer: *const AHardwareBuffer, out_desc: *mut AHardwareBuffer_Desc);

pub type ToHardwareBufferFn =
    unsafe extern "C" fn(env: *mut JNIEnv, hardware_buffer: *mut AHardwareBuffer) -> jobject;

#[derive(Clone, Copy)]
pub struct HardwareBufferApi {
    pub allocate: AllocateFn,
    pub is_supported: IsSupportedFn,
    pub release: ReleaseFn,
    pub unlock: UnlockFn,
    pub lock: LockFn,
    pub describe: DescribeFn,
    pub to_hardware_buffer: ToHardwareBufferFn,
}

// SAFETY: all contained values are plain function pointers — inherently Send + Sync.
unsafe impl Send for HardwareBufferApi {}
unsafe impl Sync for HardwareBufferApi {}

static API: OnceLock<Option<HardwareBufferApi>> = OnceLock::new();

pub(crate) const MIN_HARDWARE_BUFFER_OS: i32 = 29;

/// Returns the loaded API, or `None` if the device is below API 29 or any
/// symbol is missing.  Thread-safe; resolves symbols exactly once.
pub(crate) fn load_hardware_buffer_api() -> Option<&'static HardwareBufferApi> {
    API.get_or_init(|| {
        if android_api_level() < MIN_HARDWARE_BUFFER_OS {
            return None;
        }
        // SAFETY: RTLD_DEFAULT is always valid; we check every pointer before use.
        unsafe { try_load() }
    })
    .as_ref()
}

unsafe fn try_load() -> Option<HardwareBufferApi> {
    Some(HardwareBufferApi {
        allocate: unsafe { sym("AHardwareBuffer_allocate") }?,
        is_supported: unsafe { sym("AHardwareBuffer_isSupported") }?,
        release: unsafe { sym("AHardwareBuffer_release") }?,
        unlock: unsafe { sym("AHardwareBuffer_unlock") }?,
        lock: unsafe { sym("AHardwareBuffer_lock") }?,
        describe: unsafe { sym("AHardwareBuffer_describe") }?,
        to_hardware_buffer: unsafe { sym("AHardwareBuffer_toHardwareBuffer") }?,
    })
}

/// Resolves a symbol from the already-loaded libraries (RTLD_DEFAULT).
unsafe fn sym<T: Copy>(name: &str) -> Option<T> {
    assert_eq!(
        size_of::<T>(),
        size_of::<*mut std::ffi::c_void>(),
        "T must be a function pointer"
    );
    unsafe {
        let cname = std::ffi::CString::new(name).ok()?;
        let ptr = libc::dlsym(libc::RTLD_DEFAULT, cname.as_ptr());
        if ptr.is_null() {
            return None;
        }
        Some(std::mem::transmute_copy(&ptr))
    }
}

fn android_api_level() -> i32 {
    #[cfg(target_os = "android")]
    {
        use std::ffi::CStr;
        // __system_property_get is available on all Android NDK targets.
        let mut buf = [0i8; 92];
        let key = c"ro.build.version.sdk";
        let len = unsafe { libc::__system_property_get(key.as_ptr(), buf.as_mut_ptr().cast()) };
        if len > 0 {
            let cstr = unsafe { CStr::from_ptr(buf.as_ptr().cast()) };
            cstr.to_str().ok().and_then(|s| s.parse().ok()).unwrap_or(0)
        } else {
            0
        }
    }
    #[cfg(not(target_os = "android"))]
    {
        0
    }
}

impl HardwareBufferApi {
    pub fn allocate_buffer(
        &self,
        desc: &AHardwareBuffer_Desc,
    ) -> Result<*mut AHardwareBuffer, i32> {
        let mut buf: *mut AHardwareBuffer = std::ptr::null_mut();
        let ret = unsafe { (self.allocate)(desc as *const _, &mut buf) };
        if ret == 0 { Ok(buf) } else { Err(ret) }
    }

    /// Allocates a buffer wrapped in an [`OwnedHardwareBuffer`], which releases
    /// its reference automatically when dropped.
    pub fn allocate_owned(&self, desc: &AHardwareBuffer_Desc) -> Result<OwnedHardwareBuffer, i32> {
        let buffer = self.allocate_buffer(desc)?;
        // SAFETY: `allocate_buffer` returned a freshly-allocated buffer holding
        // a single reference that we now take ownership of.
        Ok(unsafe { OwnedHardwareBuffer::from_raw(*self, buffer) })
    }

    pub fn is_supported(&self, desc: &AHardwareBuffer_Desc) -> bool {
        unsafe { (self.is_supported)(desc as *const _) != 0 }
    }

    /// # Safety
    /// `buffer` must be a valid, living `AHardwareBuffer`.
    pub unsafe fn release(&self, buffer: *mut AHardwareBuffer) {
        unsafe {
            (self.release)(buffer);
        }
    }

    /// # Safety
    /// `buffer` must be locked before calling.
    pub unsafe fn unlock(
        &self,
        buffer: *mut AHardwareBuffer,
        fence: Option<&mut i32>,
    ) -> Result<(), i32> {
        let fence_ptr = fence.map_or(std::ptr::null_mut(), |f| f as *mut _);
        let ret = unsafe { (self.unlock)(buffer, fence_ptr) };
        if ret == 0 { Ok(()) } else { Err(ret) }
    }

    /// # Safety
    /// `buffer` must outlive the returned virtual address.
    pub unsafe fn lock(
        &self,
        buffer: *mut AHardwareBuffer,
        usage: u64,
        fence: i32,
        rect: Option<&ARect>,
    ) -> Result<*mut std::ffi::c_void, i32> {
        let mut addr: *mut std::ffi::c_void = std::ptr::null_mut();
        let rect_ptr = rect.map_or(std::ptr::null(), |r| r as *const _);
        let ret = unsafe { (self.lock)(buffer, usage, fence, rect_ptr, &mut addr) };
        if ret == 0 { Ok(addr) } else { Err(ret) }
    }

    pub fn describe(&self, buffer: *const AHardwareBuffer) -> AHardwareBuffer_Desc {
        let mut desc = unsafe { std::mem::zeroed() };
        unsafe { (self.describe)(buffer, &mut desc) };
        desc
    }

    /// # Safety
    /// `env` must be a valid JNI environment for the current thread.
    pub unsafe fn to_java_hardware_buffer(
        self,
        env: *mut JNIEnv,
        buffer: *mut AHardwareBuffer,
    ) -> jobject {
        unsafe { (self.to_hardware_buffer)(env, buffer) }
    }
}

/// Loads the API (if available on this device) and allocates a self-releasing
/// buffer.
///
/// Returns `None` if hardware buffers are unsupported (API level < 29 or a
/// missing symbol), otherwise the allocation `Result`.
pub fn allocate_hardware_buffer(
    desc: &AHardwareBuffer_Desc,
) -> Option<Result<OwnedHardwareBuffer, i32>> {
    Some(load_hardware_buffer_api()?.allocate_owned(desc))
}

#[inline]
fn rgba8888_gpu_usage() -> u64 {
    AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE.0
}

#[inline]
fn rgba8888_cpu_write_usage() -> u64 {
    AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN.0
}

#[inline]
fn rgba8888_hardware_buffer_usage() -> u64 {
    rgba8888_gpu_usage() | rgba8888_cpu_write_usage()
}

#[inline]
fn rgba8888_lock_usage() -> u64 {
    rgba8888_cpu_write_usage()
}

#[inline]
fn rgba8888_descriptor(width: usize, height: usize, usage: u64) -> AHardwareBuffer_Desc {
    AHardwareBuffer_Desc {
        width: width as u32,
        height: height as u32,
        layers: 1,
        format: AHardwareBuffer_Format::AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM.0,
        usage,
        stride: 0,
        rfu0: 0,
        rfu1: 0,
    }
}

#[inline]
fn checked_rgba8_row_bytes(width: usize) -> Result<usize, anyhow::Error> {
    width
        .checked_mul(4)
        .ok_or_else(|| anyhow::anyhow!("RGBA row byte count overflow for width {width}"))
}

fn hardware_buffer_stride_bytes(
    owned: &OwnedHardwareBuffer,
    width: usize,
    height: usize,
) -> Result<(usize, usize, usize), anyhow::Error> {
    let row_bytes = checked_rgba8_row_bytes(width)?;
    let desc = owned.describe();
    let stride_pixels = desc.stride as usize;
    if stride_pixels < width {
        return Err(anyhow::anyhow!(
            "Hardware buffer stride {stride_pixels} is smaller than image width {width}"
        ));
    }
    let stride_bytes = stride_pixels.checked_mul(4).ok_or_else(|| {
        anyhow::anyhow!("Hardware buffer row stride byte count overflow: {stride_pixels} pixels")
    })?;
    let mapped_len = stride_bytes.checked_mul(height).ok_or_else(|| {
        anyhow::anyhow!(
            "Hardware buffer mapped byte count overflow: stride={stride_bytes}, height={height}"
        )
    })?;
    Ok((row_bytes, stride_bytes, mapped_len))
}

fn lock_rgba8888_hardware_buffer(
    owned: &mut OwnedHardwareBuffer,
    width: usize,
    height: usize,
) -> Result<(LockGuard<'_>, usize, usize, usize), anyhow::Error> {
    let (row_bytes, stride_bytes, mapped_len) = hardware_buffer_stride_bytes(owned, width, height)?;
    let rect = ARect {
        left: 0,
        top: 0,
        right: width as i32,
        bottom: height as i32,
    };
    let lock = owned
        .lock(rgba8888_lock_usage(), -1, Some(&rect))
        .map_err(|x| anyhow::anyhow!("Locking hardware buffer failed with an error {x}"))?;
    if lock.as_ptr().is_null() {
        return Err(anyhow::anyhow!(
            "Locking hardware buffer returned a null virtual address"
        ));
    }
    Ok((lock, row_bytes, stride_bytes, mapped_len))
}

pub(crate) fn create_rgba8888_hardware_buffer(
    env: &mut Env,
    data: &[u8],
    width: usize,
    height: usize,
) -> Result<Option<jobject>, anyhow::Error> {
    let Some(hw_apis) = load_hardware_buffer_api() else {
        return Ok(None);
    };

    let descriptor = rgba8888_descriptor(width, height, rgba8888_hardware_buffer_usage());
    if !hw_apis.is_supported(&descriptor) {
        return Ok(None);
    }

    let mut owned = hw_apis
        .allocate_owned(&descriptor)
        .map_err(|x| anyhow::anyhow!("Allocation hardware buffer failed with an error {x}"))?;

    let (mut lock, row_bytes, stride_bytes, mapped_len) =
        lock_rgba8888_hardware_buffer(&mut owned, width, height)?;
    let expected_len = row_bytes.checked_mul(height).ok_or_else(|| {
        anyhow::anyhow!("RGBA source byte count overflow: row={row_bytes}, height={height}")
    })?;
    if data.len() != expected_len {
        return Err(anyhow::anyhow!(
            "Invalid RGBA source length: got {}, expected {} for {}x{}",
            data.len(),
            expected_len,
            width,
            height
        ));
    }

    let dst = unsafe { std::slice::from_raw_parts_mut(lock.as_mut_ptr().cast::<u8>(), mapped_len) };
    for (src_row, dst_row) in data
        .chunks_exact(row_bytes)
        .zip(dst.chunks_exact_mut(stride_bytes))
    {
        dst_row[..row_bytes].copy_from_slice(src_row);
    }
    drop(lock);

    Ok(Some(unsafe { owned.to_java_hardware_buffer(env) }))
}

pub(crate) fn create_rgba8888_hardware_buffer_from_u16(
    env: &mut Env,
    data: &[u16],
    width: usize,
    height: usize,
    bit_depth: usize,
) -> Result<Option<jobject>, anyhow::Error> {
    let Some(hw_apis) = load_hardware_buffer_api() else {
        return Ok(None);
    };

    let descriptor = rgba8888_descriptor(width, height, rgba8888_hardware_buffer_usage());
    if !hw_apis.is_supported(&descriptor) {
        return Ok(None);
    }

    let mut owned = hw_apis
        .allocate_owned(&descriptor)
        .map_err(|x| anyhow::anyhow!("Allocation hardware buffer failed with an error {x}"))?;

    let (mut lock, row_bytes, stride_bytes, mapped_len) =
        lock_rgba8888_hardware_buffer(&mut owned, width, height)?;
    let src_stride = width
        .checked_mul(4)
        .ok_or_else(|| anyhow::anyhow!("RGBA source row length overflow for width {width}"))?;
    let expected_len = src_stride.checked_mul(height).ok_or_else(|| {
        anyhow::anyhow!("RGBA source pixel count overflow: row={src_stride}, height={height}")
    })?;
    if data.len() != expected_len {
        return Err(anyhow::anyhow!(
            "Invalid high-bit-depth RGBA source length: got {}, expected {} for {}x{}",
            data.len(),
            expected_len,
            width,
            height
        ));
    }

    let shift = bit_depth.saturating_sub(8) as u32;
    let dst = unsafe { std::slice::from_raw_parts_mut(lock.as_mut_ptr().cast::<u8>(), mapped_len) };
    for (src_row, dst_row) in data
        .chunks_exact(src_stride)
        .zip(dst.chunks_exact_mut(stride_bytes))
    {
        for (dst, &src) in dst_row[..row_bytes].iter_mut().zip(src_row.iter()) {
            *dst = (src >> shift).min(255) as u8;
        }
    }
    drop(lock);

    Ok(Some(unsafe { owned.to_java_hardware_buffer(env) }))
}

/// An owned `AHardwareBuffer` that releases its reference automatically on drop.
///
/// Construct via [`HardwareBufferApi::allocate_owned`] or
/// [`allocate_hardware_buffer`]. Lock it with [`OwnedHardwareBuffer::lock`],
/// which hands back a [`LockGuard`] that unlocks itself on drop.
pub struct OwnedHardwareBuffer {
    api: HardwareBufferApi,
    buffer: *mut AHardwareBuffer,
}

// SAFETY: `AHardwareBuffer` is an atomically reference-counted, thread-safe
// handle, so moving ownership across threads is sound. `Sync` is intentionally
// *not* implemented: concurrent locking must be serialized by the caller, which
// the `&mut self` receiver on `lock` already guarantees for a single owner.
unsafe impl Send for OwnedHardwareBuffer {}

impl OwnedHardwareBuffer {
    /// Wraps a raw buffer, taking ownership of exactly one reference.
    ///
    /// # Safety
    /// `buffer` must be a valid `AHardwareBuffer` whose reference count this
    /// value becomes responsible for releasing exactly once. `api` must be a
    /// correctly loaded function table.
    pub unsafe fn from_raw(api: HardwareBufferApi, buffer: *mut AHardwareBuffer) -> Self {
        Self { api, buffer }
    }

    /// Borrows the underlying pointer without giving up ownership.
    #[inline]
    pub fn as_raw(&self) -> *mut AHardwareBuffer {
        self.buffer
    }

    /// Consumes the wrapper and returns the raw pointer **without** releasing.
    /// The caller becomes responsible for calling `AHardwareBuffer_release`.
    #[inline]
    pub fn into_raw(self) -> *mut AHardwareBuffer {
        let this = std::mem::ManuallyDrop::new(self);
        this.buffer
    }

    /// Returns the buffer's dimensions, format and usage flags.
    pub fn describe(&self) -> AHardwareBuffer_Desc {
        let mut desc = unsafe { std::mem::zeroed() };
        // SAFETY: `self.buffer` is a valid, living buffer for as long as `self`.
        unsafe { (self.api.describe)(self.buffer, &mut desc) };
        desc
    }

    /// Locks the buffer for direct CPU access and returns a [`LockGuard`] that
    /// unlocks automatically when it goes out of scope.
    ///
    /// `usage` is a bitmask of `AHARDWAREBUFFER_USAGE_CPU_*` flags, `fence` is a
    /// sync-fence fd to wait on before the contents are valid (or `-1` for
    /// none), and `rect` optionally restricts the locked region.
    ///
    /// Takes `&mut self`, so the borrow checker prevents the buffer from being
    /// released — or locked a second time — while a guard is alive.
    pub fn lock(
        &mut self,
        usage: u64,
        fence: i32,
        rect: Option<&ARect>,
    ) -> Result<LockGuard<'_>, i32> {
        let mut addr: *mut std::ffi::c_void = std::ptr::null_mut();
        let rect_ptr = rect.map_or(std::ptr::null(), |r| r as *const _);
        // SAFETY: valid buffer; `addr` is a valid out-pointer.
        let ret = unsafe { (self.api.lock)(self.buffer, usage, fence, rect_ptr, &mut addr) };
        if ret == 0 {
            Ok(LockGuard {
                api: self.api,
                buffer: self.buffer,
                addr,
                _owner: PhantomData,
            })
        } else {
            Err(ret)
        }
    }

    /// Locks the whole buffer with no fence, runs `f` against the mapped
    /// address, then unlocks. The unlock runs even if `f` panics, because the
    /// guard unlocks on drop (including while unwinding).
    pub fn with_lock<R>(
        &mut self,
        usage: u64,
        f: impl FnOnce(*mut std::ffi::c_void) -> R,
    ) -> Result<R, i32> {
        let guard = self.lock(usage, -1, None)?;
        Ok(f(guard.addr))
        // `guard` drops here → AHardwareBuffer_unlock.
    }

    /// Converts the buffer into a Java `HardwareBuffer` object.
    ///
    /// # Safety
    /// `env` must be a valid JNI environment for the current thread.
    pub unsafe fn to_java_hardware_buffer(&self, env: &mut Env) -> jobject {
        unsafe { (self.api.to_hardware_buffer)(env.get_raw(), self.buffer) }
    }
}

impl Drop for OwnedHardwareBuffer {
    fn drop(&mut self) {
        // SAFETY: we own exactly one reference to a valid buffer; releasing it
        // here balances the allocation/acquire.
        unsafe { (self.api.release)(self.buffer) };
    }
}

/// RAII guard returned by [`OwnedHardwareBuffer::lock`]. Calls
/// `AHardwareBuffer_unlock` when dropped.
///
/// While this guard is alive it mutably borrows the owning buffer, so the
/// buffer cannot be released or re-locked until the guard is gone.
pub struct LockGuard<'a> {
    api: HardwareBufferApi,
    buffer: *mut AHardwareBuffer,
    addr: *mut std::ffi::c_void,
    _owner: PhantomData<&'a mut OwnedHardwareBuffer>,
}

impl LockGuard<'_> {
    /// The mapped, CPU-visible address of the locked contents.
    #[inline]
    pub fn as_ptr(&self) -> *const std::ffi::c_void {
        self.addr
    }

    /// The mapped, CPU-visible address for writing.
    #[inline]
    pub fn as_mut_ptr(&mut self) -> *mut std::ffi::c_void {
        self.addr
    }

    /// Reinterprets the mapping as a byte slice of length `len`.
    ///
    /// # Safety
    /// `len` must not exceed the size of the locked region, and the buffer must
    /// have been locked with a CPU-read usage flag.
    pub unsafe fn as_slice(&self, len: usize) -> &[u8] {
        unsafe { std::slice::from_raw_parts(self.addr.cast::<u8>(), len) }
    }

    /// Reinterprets the mapping as a mutable byte slice of length `len`.
    ///
    /// # Safety
    /// As [`as_slice`](Self::as_slice), and the buffer must have been locked
    /// with a CPU-write usage flag.
    pub unsafe fn as_mut_slice(&mut self, len: usize) -> &mut [u8] {
        unsafe { std::slice::from_raw_parts_mut(self.addr.cast::<u8>(), len) }
    }

    /// Explicitly unlocks, retrieving the optional release sync-fence fd
    /// produced by the driver (`-1` if none).
    ///
    /// On error the buffer is left locked and the guard is consumed, so no
    /// further automatic unlock occurs.
    pub fn unlock_with_fence(self) -> Result<i32, i32> {
        let this = std::mem::ManuallyDrop::new(self);
        let mut fence: i32 = -1;
        // SAFETY: the guard only exists while the buffer is locked.
        let ret = unsafe { (this.api.unlock)(this.buffer, &mut fence) };
        if ret == 0 { Ok(fence) } else { Err(ret) }
    }
}

impl Drop for LockGuard<'_> {
    fn drop(&mut self) {
        // SAFETY: the guard only exists while the buffer is locked. The fence
        // and any error are ignored because Drop cannot report them — callers
        // that need the fence should use `unlock_with_fence`.
        unsafe { (self.api.unlock)(self.buffer, std::ptr::null_mut()) };
    }
}
