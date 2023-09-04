# AVIF/HEIF Coder for Android 24+

Library provides simple interface to decode or encode ( create ) AVIF and HEIF images for Android
Very fast and convinient to use AVIF in android apps with api version 24+
26+. Based on libheif, libde265, libx265, libyuv, libaom and libdav1d

Correctly handles ICC, and color profiles. Fully supports HDR images, 10, 12 bit
The most features AVIF, HEIF library in android

# Usage example

```kotlin
// May decode AVIF(AV1) and HEIC (HEVC) images, all HDR images currently will be converted in 8bit
val bitmap: Bitmap = HeifCoder().decode(buffer) // Decode avif from ByteArray
val bytes: ByteArray = HeifCoder().encodeAvif(decodedBitmap) // Encode Bitmap to AVIF
val bytes = HeifCoder().encodeHeic(bitmap) // Encode Bitmap to HEIC / Supports HDR in RGBA_F16
// Check if image is valid AVIF(AV1) image
val isAvif = HeifCoder().isAvif(byteArray)
// Check if image is valid HEIF(HEVC) image
val isHeif = HeifCoder().isAvif(byteArray)
// Check if image is AVIF or HEIF, just supported one
val isImageSupported = HeifCoder().isSupportedImage(byteArray)
// Get image size ( this call never throw)
val imageSize: Size? = HeifCoder().getSize(byteArray)
// Decode AVIF or HEIF in sample size if needed
val bitmap: Bitmap = decodeSampled(byteArray, scaledWidth, scaledHeight)
```

# Add Jitpack repository

```groovy
repositories {
    maven { url "https://jitpack.io" }
}
```

```groovy
implementation 'com.github.awxkee:avif-coder:1.1.1' // or any version above picker from release tags
```

# Also supports coil integration

Just add to image loader heif decoder factory and use it as image loader in coil

```kotlin
val imageLoader = ImageLoader.Builder(context)
    .components {
        add(HeifDecoder.Factory())
    }
    .build()
```

# Self-build

## Requirements

libdav1d:

- ndk
- meson
- ninja
- cmake
- nasm

libyuv, de265, x265, aom:

- ndk
- ninja
- cmake
- nasm

libheif:
- ndk
- ninja
- cmake
- and all built libraries above

If you wish to build by yourself you may use ready `build_aom.sh`
script, `build_dav1d.sh`, `build_x265.sh`, `build_de265.sh`, `build_yuv.sh`, `build_heif.sh` or you
may use `build_all.sh`

**All commands are require the NDK path set by NDK_PATH environment variable**

* If you wish to build for **x86** you have to add a **$INCLUDE_X86** environment variable for
  example:*

```shell
NDK_PATH=/path/to/ndk INCLUDE_X86=yes bash build_aom.sh
```

# Disclaimer

## AVIF

AVIF is the next step in image optimization based on the AV1 video codec. It is standardized by the
Alliance for Open Media. AVIF offers more compression gains than other image formats such as JPEG
and WebP. Our and other studies have shown that, depending on the content, encoding settings, and
quality target, you can save up to 50% versus JPEG or 20% versus WebP images. The advanced image
encoding feature of AVIF brings codec and container support for HDR and wide color gamut images,
film grain synthesis, and progressive decoding. AVIF support has improved significantly since Chrome
M85 implemented AVIF support last summer.
