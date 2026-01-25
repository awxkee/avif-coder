# Proposal: Decoder-Only Release

## Summary

Proposing a **decoder-only release** that removes all encoding functionality while maintaining full decoding capabilities. This results in a **60-70% reduction in native library size** (from ~12-14 MB to ~3.5-4.5 MB per ABI).

**Branch:** `decoder-only-release`  
**Status:** Ready for review

---

## Motivation

Many Android apps only need to decode AVIF/HEIC images for display and don't require encoding. The encoder libraries (libaom and libx265) add ~6-8 MB per ABI that is unnecessary for decoder-only use cases.

**Benefits:**
- 60-70% smaller library size
- Faster build times
- Android 15+ compatibility (16 KB page size alignment)
- Focused functionality for decoder-only apps

---

## What Changed

**Removed:**
- Encoding APIs (`encodeAvif()`, `encodeHeic()`)
- Encoder libraries (libaom, x265, SVT-AV1, kvazaar)
- Encoder build scripts and code

**Retained:**
- Full AVIF/HEIC decoding support
- HDR image support (10-bit, 12-bit)
- Wide color gamut and ICC profile handling
- Animated AVIF support
- Coil integration module

---

## Library Size

| ABI | Before | After | Savings |
|-----|--------|-------|---------|
| arm64-v8a | ~12-14 MB | ~3.5 MB | ~60-70% |
| armeabi-v7a | ~10-12 MB | ~2.6 MB | ~60-70% |
| x86 | ~14-16 MB | ~3.8 MB | ~60-70% |
| x86_64 | ~15-17 MB | ~4.5 MB | ~60-70% |

---

## API Changes

**Removed:**
```kotlin
HeifCoder().encodeAvif(...)
HeifCoder().encodeHeic(...)
```

**Available (unchanged):**
```kotlin
HeifCoder().decode(buffer)
HeifCoder().decodeSampled(buffer, width, height)
HeifCoder().getSize(buffer)
HeifCoder().isAvif(buffer)
HeifCoder().isHeif(buffer)
HeifCoder().isSupportedImage(buffer)
```

---

## Migration

Apps using encoding can:
1. Stay on `master` branch (full functionality)
2. Use alternative encoding solutions (Android ImageWriter, server-side)
3. Build from source if needed

---

## Testing

✅ All decoding functionality verified:
- AVIF/HEIC decoding
- HDR images
- ICC profiles
- Animated AVIF
- Coil integration

---

## Questions

1. Versioning strategy? (separate tag vs. new major version)
2. Branch strategy? (maintain both branches long-term?)
3. Artifact naming? (separate artifact ID for decoder-only?)

---

**Branch:** `decoder-only-release`  
**Documentation:** See `DECODER_ONLY_SUMMARY.md` for full details
