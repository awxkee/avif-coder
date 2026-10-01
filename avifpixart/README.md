# Building

Run `bash avifpixart/build.sh` from the repository root. The build requires nightly
Rust with `rust-src`, the Android targets, and the NDK linkers configured in
`.cargo/config.toml`. It does not change the default Rust toolchain.

AArch64 uses optimization level `3`; ARMv7, x86 and x86_64 use `z`. All targets use
full LTO, one codegen unit, symbol stripping, and `panic = "unwind"`.

The script rebuilds `std` with `panic-unwind` and without `backtrace`. It also sets
`-Z location-detail=none` and `-Z fmt-debug=shallow`. Panics still unwind, run
destructors, and can be caught, but panic source locations and automatic panic
backtraces are unavailable, and derived `Debug` output omits fields. Explicit
backtrace capture may still work on supported targets. Panic messages and custom
`Display` formatting remain enabled. These unstable options require a compatible
nightly toolchain.

The final CMake link uses section garbage collection, safe identical-code folding,
and `coder.map.txt` to export only JNI entry points. Add new native Java methods to
that export list. Compare stripped `libcoder.so` files when measuring shipped size;
the Rust static archive also contains code discarded by the final linker.

## Panic boundaries

Keep Rust exports as `extern "C"`, never `extern "C-unwind"` for JNI. Encoding and
decoding run inside `EnvUnowned::with_env`, which catches Rust panics; the wrappers
translate them to Java `RuntimeException`s before returning null. Initialization
and logging are also inside that catch boundary. Metadata and container detection
catch panics and return their existing failure sentinels.

Low-level pixel conversion functions have no recoverable error channel. A panic
escaping one of those non-unwinding C ABI functions aborts the process, as defined
by Rust; it does not unwind through C++ or the JVM. Allocation failure, a panic in
a panic hook/destructor during unwinding, or a failure in panic reporting may also
abort. Valid pointers, buffer sizes and enum values remain requirements of the
unsafe C API; catching panics cannot repair invalid memory accesses.

Run the portable boundary regression tests without an Android device:

```sh
rustc +nightly --edition=2024 --test avifpixart/tests/ffi_contract.rs \
  -C opt-level=3 -C panic=unwind -C lto=fat -C codegen-units=1 \
  -Z location-detail=none -Z fmt-debug=shallow -o /tmp/avif-ffi-contract
/tmp/avif-ffi-contract
```

## Header generation

```sh
cbindgen --config cbindgen.toml --crate avifweaver --output include/avifweaver.h
```
