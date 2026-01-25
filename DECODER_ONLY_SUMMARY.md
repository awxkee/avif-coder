# Decoder-Only Release Summary

## Overview

This document summarizes the decoder-only release of the AVIF/HEIF Coder library for Android. This release removes all encoding functionality (AVIF and HEIC encoding) while maintaining full decoding capabilities, resulting in a significantly smaller library footprint.

**Branch:** `decoder-only-release`  
**Status:** ✅ Ready for Release  
**Date:** January 2026

---

## What Changed

### Removed Components

#### 1. Encoding APIs
- **Kotlin API:** Removed `encodeAvif()` and `encodeHeic()` methods from `HeifCoder` class
- **JNI Layer:** Deleted `JniEncoder.cpp` (945 lines)
- **External Functions:** Removed all encoder-related external function declarations

#### 2. Native Encoder Libraries
- **libaom** (AV1 encoder) - Removed from all ABIs (~4-7 MB per ABI)
- **x265** (HEVC encoder) - Removed from all ABIs (~1.7-2.4 MB per ABI)
- **SVT-AV1** (AV1 encoder) - Build script removed
- **kvazaar** (HEVC encoder) - Build script removed

#### 3. Build Infrastructure
- Deleted encoder build scripts:
  - `build_aom.sh`
  - `build_x265.sh`
  - `build_svt.sh`
  - `build_kvazaar.sh`
- Updated `build_all.sh` to exclude encoder builds
- Updated `build_heif.sh` to disable encoder support (`WITH_X265=OFF`, `WITH_KVAZAAR=OFF`)

#### 4. Code and Headers
- Removed AOM encoder headers (`avif-coder/src/main/cpp/avif/aom/` directory)
- Removed `codec_aom.c` from avif_shared library
- Removed AOM codec references from `avif.c` and `avif.h`
- Cleaned up CMake configurations to exclude encoder dependencies

### Retained Components

#### 1. Full Decoding Support
- ✅ AVIF (AV1) decoding via libdav1d
- ✅ HEIC/HEIF (HEVC) decoding via libde265
- ✅ HDR image support (10-bit, 12-bit)
- ✅ Wide color gamut and ICC profile handling
- ✅ Animated AVIF support
- ✅ All pixel format conversions

#### 2. Native Decoder Libraries
- **libdav1d** - AV1 decoder (697 KB - 1.5 MB per ABI)
- **libde265** - HEVC decoder (1.0 MB - 1.6 MB per ABI)
- **libheif** - HEIF/AVIF container handler (898 KB - 1.4 MB per ABI)
- **libyuv** - YUV conversion utilities
- **libsharpyuv** - Sharp YUV conversion

#### 3. Enhanced Features
- ✅ `getSize()` method implementation (previously missing)
- ✅ Improved error logging and debugging
- ✅ Android 15+ compatibility (16 KB page size alignment)
- ✅ Coil integration module (`avif-coder-coil`)

---

## Library Size Reduction

### Before (with encoders)
- **arm64-v8a:** ~12-14 MB (estimated)
- **armeabi-v7a:** ~10-12 MB (estimated)
- **x86:** ~14-16 MB (estimated)
- **x86_64:** ~15-17 MB (estimated)

### After (decoder-only)
- **arm64-v8a:** ~3.5 MB (libcoder.so + decoder libs)
- **armeabi-v7a:** ~2.6 MB
- **x86:** ~3.8 MB
- **x86_64:** ~4.5 MB

### Size Savings
- **~60-70% reduction** in native library size
- **~6-8 MB saved per ABI** (removed libaom + libx265)

---

## Technical Details

### Android 15+ Compatibility

All native libraries are built with 16 KB page size alignment to support Android 15+ devices:

```cmake
target_link_options(coder PRIVATE 
    "-Wl,-z,max-page-size=16384"
    "-Wl,-z,common-page-size=16384"
    "-Wl,-z,relro"
    "-Wl,-z,now"
)
```

### Build Configuration

#### CMake Changes
- Removed `JniEncoder.cpp` from source list
- Removed `libaom` and `x265` imported libraries
- Updated `avif_shared` to only include `AVIF_CODEC_DAV1D`
- Removed AOM encoder codec from avif library

#### Build Scripts
All build scripts updated to use consistent NDK path:
```bash
export NDK_PATH="$HOME/Library/Android/sdk/ndk/28.1.13356709"
```

### API Changes

#### Removed Methods
```kotlin
// ❌ These methods are no longer available:
HeifCoder().encodeAvif(bitmap, quality, speed, ...)
HeifCoder().encodeHeic(bitmap, quality, ...)
```

#### Available Methods
```kotlin
// ✅ All decoding methods remain available:
HeifCoder().decode(buffer)
HeifCoder().decodeSampled(buffer, width, height)
HeifCoder().getSize(buffer)
HeifCoder().isAvif(buffer)
HeifCoder().isHeif(buffer)
HeifCoder().isSupportedImage(buffer)
```

---

## Usage

### Basic Decoding

```kotlin
val coder = HeifCoder()

// Decode AVIF or HEIC image
val bitmap: Bitmap = coder.decode(buffer)

// Get image size without decoding
val size: Size? = coder.getSize(buffer)

// Decode with scaling
val bitmap: Bitmap = coder.decodeSampled(
    buffer,
    scaledWidth,
    scaledHeight,
    PreferredColorConfig.RGBA_8888,
    ScaleMode.RESIZE
)

// Check image type
val isAvif = coder.isAvif(buffer)
val isHeif = coder.isHeif(buffer)
val isSupported = coder.isSupportedImage(buffer)
```

### Coil Integration

```kotlin
// Add to your ImageLoader
val imageLoader = ImageLoader.Builder(context)
    .components {
        add(HeifDecoder.Factory())
    }
    .build()

// Use with Coil
imageView.load("https://example.com/image.avif", imageLoader)
```

---

## Migration Guide

### If You Were Using Encoding

If your app previously used encoding functionality, you have these options:

1. **Use a different library for encoding:**
   - Keep this library for decoding only
   - Use another library (e.g., Android's built-in ImageWriter for HEIC on API 28+)
   - Use server-side encoding

2. **Stay on the master branch:**
   - The `master` branch still contains full encoder/decoder functionality
   - Use `decoder-only-release` branch for decoder-only builds

3. **Build from source:**
   - You can rebuild the encoder libraries if needed
   - See `BUILD_INSTRUCTIONS.md` for details

### Code Changes Required

**Before:**
```kotlin
val coder = HeifCoder()
val encodedBytes = coder.encodeAvif(bitmap, quality = 80)
```

**After:**
```kotlin
// Encoding removed - use alternative solution
// Decoding still works:
val coder = HeifCoder()
val bitmap = coder.decode(imageBytes)
```

---

## Building

### Prerequisites

- Android NDK 28.1.13356709
- CMake 3.22.1+
- Ninja
- Python 3 (for some build scripts)
- nasm (for libyuv)

### Build Native Libraries

```bash
# Build all decoder dependencies
bash build_all.sh

# Or build individually:
bash build_dav1d.sh   # AV1 decoder
bash build_de265.sh   # HEVC decoder
bash build_yuv.sh     # YUV conversion
bash build_sharpyuv.sh # Sharp YUV
bash build_heif.sh    # HEIF/AVIF container
```

### Build Android Library

```bash
# Build release AAR with native libraries
./gradlew :avif-coder:assembleRelease

# Build Coil integration module
./gradlew :avif-coder-coil:assembleRelease
```

### Output AARs

- **avif-coder-release.aar** (~9.2 MB) - Contains all native libraries
- **avif-coder-coil-release.aar** (~8.8 KB) - Coil wrapper (depends on avif-coder)

---

## Testing

### Verified Functionality

✅ AVIF decoding (all bit depths)  
✅ HEIC/HEIF decoding  
✅ HDR image support  
✅ ICC profile handling  
✅ Animated AVIF  
✅ Image size detection  
✅ Scaled decoding  
✅ Coil integration  

### Test Coverage

The demo app (`app` module) includes comprehensive test images:
- Standard AVIF/HEIC images
- HDR images (various color spaces)
- Large images
- Images with custom ICC profiles
- Animated AVIF sequences

---

## File Structure Changes

### Deleted Files
```
avif-coder/src/main/cpp/JniEncoder.cpp
avif-coder/src/main/cpp/avif/aom/ (entire directory)
build_aom.sh
build_x265.sh
build_svt.sh
build_kvazaar.sh
avif-coder/src/main/cpp/lib/*/libaom.so (all ABIs)
avif-coder/src/main/cpp/lib/*/libx265.so (all ABIs)
```

### Modified Files
```
avif-coder/src/main/java/.../HeifCoder.kt (removed encoder methods)
avif-coder/src/main/cpp/CMakeLists.txt (removed encoder libs)
avif-coder/src/main/cpp/avif/CMakeLists.txt (removed AOM codec)
build_heif.sh (disabled encoder support)
build_all.sh (removed encoder builds)
README.md (updated documentation)
```

### Added Files
```
BUILD_INSTRUCTIONS.md (build guide)
DECODER_ONLY_SUMMARY.md (this file)
```

---

## Commit History

The decoder-only release consists of 9 commits:

1. **Remove encoder functionality** - Initial encoder removal
2. **Clean up encoder library headers** - Remove AOM headers
3. **Remove unused encoder build scripts** - Delete build scripts
4. **Remove commented-out encoder code** - Clean MainActivity
5. **Update build scripts** - Fix NDK paths
6. **Add 16 KB page size alignment** - Android 15+ compatibility
7. **Fix image loading issues** - Implement getSize() and fix filters
8. **Fix throwException calls** - Use std::string objects
9. **Add avif-coder-coil module** - Coil integration

---

## Benefits

### 1. Smaller Library Size
- 60-70% reduction in native library size
- Faster app downloads and updates
- Lower memory footprint

### 2. Faster Build Times
- Fewer dependencies to build
- Simpler build process
- Reduced CI/CD build time

### 3. Better Compatibility
- Android 15+ 16 KB page size support
- Cleaner dependency chain
- Fewer potential conflicts

### 4. Focused Functionality
- Clear separation: decoding only
- Easier to maintain
- Better for apps that only need decoding

---

## Limitations

### What's Not Available

❌ AVIF encoding  
❌ HEIC encoding  
❌ Encoding quality/speed controls  
❌ Encoding-specific features  

### Workarounds

- Use Android's built-in `ImageWriter` for HEIC encoding (API 28+)
- Use server-side encoding for AVIF
- Use other libraries for encoding needs
- Stay on `master` branch for full functionality

---

## Support

### Issues and Questions

- **GitHub Issues:** Report issues on the repository
- **Documentation:** See `README.md` and `BUILD_INSTRUCTIONS.md`
- **Branch Comparison:** Compare `decoder-only-release` vs `master` for full changes

### Contributing

When contributing to the decoder-only branch:
- Do not add encoder functionality
- Focus on decoder improvements
- Maintain Android 15+ compatibility
- Keep library size minimal

---

## Conclusion

The decoder-only release provides a streamlined, lightweight solution for AVIF/HEIC decoding on Android. By removing encoding functionality, the library is:

- **Smaller** - 60-70% size reduction
- **Faster** - Fewer dependencies, faster builds
- **Compatible** - Android 15+ ready
- **Focused** - Clear purpose: decoding only

This release is ideal for apps that:
- Only need to display AVIF/HEIC images
- Want minimal library size
- Don't require encoding functionality
- Need Android 15+ compatibility

For apps requiring encoding, use the `master` branch or alternative solutions.

---

**Last Updated:** January 2026  
**Branch:** `decoder-only-release`  
**Version:** Decoder-Only v1.0
