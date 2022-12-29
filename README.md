# AVIF/HEIF Coder for Android 24+

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