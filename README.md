# AVIF/HEIF Coder for Android 24+

# Usage example
```kotlin
// May decode AVIF(AV1) only
val bitmap: Bitmap = HeifCoder().decodeAvif(buffer)
val bytes: ByteArray = HeifCoder().encodeAvif(decodedBitmap)
val bytes = HeifCoder().encodeHeic(bitmap)
```