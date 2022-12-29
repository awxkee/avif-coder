# AVIF/HEIF Coder for Android 26+

Library provides simple interface to decode or encode ( create ) AVIF and HEIF images for Android 26+.
Based on libheif, libde265, libx265, libyuv and libaom

# Usage example

```kotlin
// May decode AVIF(AV1) and HEIC (HEVC) images
val bitmap: Bitmap = HeifCoder().decode(buffer) // Decode avif from ByteArray
val bytes: ByteArray = HeifCoder().encodeAvif(decodedBitmap) // Encode Bitmap to AVIF
val bytes = HeifCoder().encodeHeic(bitmap) // Encode Bitmap to HEIC
// Check if image is valid AVIF(AV1) image
val isAvif = HeifCoder().isAvif(byteArray)
// Check if image is valid HEIF(HEVC) image
val isHeif = HeifCoder().isAvif(byteArray)
// Check if image is AVIF or HEIF, just supported one
val isImageSupported = HeifCoder().isSupportedImage(byteArray)
```

```groovy
implementation 'com.github.awxkee:avif-coder:1.0.9' // or any version above picker from release tags
```

# Disclaimer
## AVIF
AVIF is the next step in image optimization based on the AV1 video codec. It is standardized by the Alliance for Open Media. AVIF offers more compression gains than other image formats such as JPEG and WebP. Our and other studies have shown that, depending on the content, encoding settings, and quality target, you can save up to 50% versus JPEG or 20% versus WebP images. The advanced image encoding feature of AVIF brings codec and container support for HDR and wide color gamut images, film grain synthesis, and progressive decoding. AVIF support has improved significantly since Chrome M85 implemented AVIF support last summer.