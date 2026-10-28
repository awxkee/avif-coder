# AVIF/HEIF Coder for Android 24+

This library provides a simple interface to decode or encode (create) AVIF and HEIF images for
Android.
Very fast and convenient to use AVIF in Android apps with API version 24+. Based
on [tealdust](https://github.com/awxkee/tealdust), [maroontree](https://github.com/awxkee/maroontree), [hpvca](https://github.com/awxkee/hpvca), [hpvcd](https://github.com/awxkee/hpvcd),
libavif and libdav1d.

Correctly handles ICC, color profiles, and HDR images.
Fully supports HDR images, 10, 12 bit. Preprocesses images in tiles to increase speed.
Extremely fast in decoding large HDR images or just large images.
The most feature-rich AVIF, HEIF library in android.
Supports decoding to all necessary pixel formats in Android and avoids Android decoding bugs.

## Gainmaps

On Android 14 (API 34) and later, decoded AVIF (AV1) gainmaps and Apple HEIC gainmaps
with `HDRGainMapHeadroom` metadata are attached to the returned `Bitmap`.
Sampled decoding applies the corresponding resize or crop to the gainmap.
Earlier Android versions decode the base image without accessing gainmap APIs.
Android 16 (API 36) also preserves HDR-to-SDR direction and alternative image
primaries. HDR-base AVIFs on Android 14/15 return the base rendition without an
attached gainmap, since those OS versions cannot express its direction.

Encoding automatically reads the input Bitmap's gainmap on API 34+:

```kotlin
val coder = Coder()
val bitmap = coder.decode(sourceBytes)
if (Build.VERSION.SDK_INT >= 34) {
    val gainmap: Gainmap? = bitmap.gainmap
    // You can also assign a Gainmap you created with the Android graphics APIs.
    // bitmap.gainmap = Gainmap(gainmapBitmap).apply { ... }
}
val avif = coder.encodeAvif(bitmap) // Includes gainmap pixels and ISO metadata.
val bytes = coder.encodeAvif(
    bitmap,
    options = AvifEncodingOptions(speed = AvSpeed.EXTRA_SLOW),
)
```

AVIF encoding supports RGB gainmaps, nonzero offsets, and 8/10/12-bit map data;
maps above 8 bits decode to `RGBA_F16`. Float Bitmap maps encode at 12 bits.
HEIC encoding supports Apple's single-channel, SDR-base gainmaps with zero
offsets and gains between 1 and the declared HDR headroom. Samples are converted
to Apple's curve; the companion ISO metadata uses the encoder's fitted curve.
Both encoding options default to `ignoreGainmapErrors = true`. Compatible maps
are preserved (including conversion to Apple's curve for HEIC); unreadable or
unsupported maps, such as HEIC maps with nonzero offsets, are omitted. If encoding
with a map fails, the encoder retries with the original base Bitmap's pixels.
Base-image encoding errors still propagate, and the input Bitmap is not modified.

Set the flag to `false` to require gainmap preservation and report an error:

```kotlin
coder.encodeHeic(bitmap, options = HevcEncodingOptions(ignoreGainmapErrors = false))
coder.encodeAvif(bitmap, options = AvifEncodingOptions(ignoreGainmapErrors = false))
```

MaroonTree does not yet encode AV2 gainmaps, and Tealdust does not expose them
during AV2 decoding. AV2 encoding ignores attached gainmaps by default; strict
mode reports them as unsupported.
Encoding requires an ARM Android ABI, as with the existing native encoders.

See Android's [Gainmap API](https://developer.android.com/reference/android/graphics/Gainmap)
for the metadata fields and display requirements.

# Usage example

```kotlin
// May decode AVIF(AV1) and HEIC (HEVC) images, HDR images supported
val bitmap: Bitmap = Coder().decode(buffer) // Decode avif from ByteArray
val bytes: ByteArray = Coder().encodeAvif(decodedBitmap) // Encode Bitmap to AVIF
val bytes =
    Coder().encodeHeic(bitmap) // Encode Bitmap to HEIC / Supports HDR in RGBA_F16, RGBA_1010102, HARDWARE
// Check if image is valid AVIF(AV1) image
val isAvif = Coder().isAvif(byteArray)
// Check if image is valid HEIF(HEVC) image
val isHeif = Coder().isHeif(byteArray)
// Check if image is AVIF or HEIF, just supported one
val isImageSupported = Coder().isSupportedImage(byteArray)
// Get image size ( this call never throw)
val imageSize: Size? = Coder().getSize(byteArray)
// Decode AVIF or HEIF in sample size if needed
val bitmap: Bitmap = decodeSampled(byteArray, scaledWidth, scaledHeight)
```

### How to add

Note versions prior 2.2.0 were published to jitpack, but this way is outdated and no longer
supported.

```kotlin
implementation("io.github.awxkee:avif-coder:2.2.0") // or any version above picker from release tags

// Coil avif plugin if you need one
implementation("io.github.awxkee:avif-coder-coil:2.2.0") // or any version above picker from release tags

// Sketch plugin if you need one 
implementation("io.github.panpf.sketch4:sketch-avif-awxkee:${SKETCH_LAST_VERSION}")

// Glide avif plugin if you need one
implementation("io.github.awxkee:avif-coder-glide:2.2.0") // or any version above picker from release tags
```

# Also supports coil integration

Just add to image loader heif decoder factory and use it as image loader in coil

```kotlin
val imageLoader = ImageLoader.Builder(context)
    .components {
        add(HeifDecoder.Factory(context))
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

libyuv:

- ndk
- ninja
- cmake
- nasm
- yasm


The bundled native dependencies are libavif 1.4.2, dav1d 1.5.3, Eigen 5.0.1,
and libyuv revision `4bc04198654971d9190475bbc9bbef8ae3b97aa7` (version 1975).
Rebuild dav1d and libyuv with `bash build_dav1d.sh` and `bash build_yuv.sh`, then
run `bash avifpixart/build.sh` for the Rust codecs. The scripts use NDK
28.2.13676358; set `NDK_PATH` to override it for dav1d/libyuv. If an existing
source checkout is at an older revision, set `DAV1D_SOURCE_DIR` or
`LIBYUV_SOURCE_DIR` to a fresh directory. Existing source checkouts are preserved.

# Disclaimer

## AVIF

AVIF is the next step in image optimization based on the AV1 video codec. It is standardized by the
Alliance for Open Media. AVIF offers more compression gains than other image formats such as JPEG
and WebP. Our and other studies have shown that, depending on the content, encoding settings, and
quality target, you can save up to 50% versus JPEG or 20% versus WebP images. The advanced image
encoding feature of AVIF brings codec and container support for HDR and wide color gamut images,
film grain synthesis, and progressive decoding. AVIF support has improved significantly since Chrome
M85 implemented AVIF support last summer.
