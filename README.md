# AVIF/HEIF Coder for Android 24+

# Usage example
```kotlin
// May decode AVIF(AV1) only
val bitmap: Bitmap = HeifCoder().decodeAvif(buffer) // Decode avif from ByteArray
val bytes: ByteArray = HeifCoder().encodeAvif(decodedBitmap) // Encode Bitmap to AVIF
val bytes = HeifCoder().encodeHeic(bitmap) // Encode Bitmap to HEIC
```

```groovy
implementation 'com.github.awxkee:avif-coder:1.0.8' // or any version above picker from release tags
```