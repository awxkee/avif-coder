# Building libheif without x265

## Issue
The existing `libheif.so` binaries were built with x265 support and have a runtime dependency on `libx265.so`, which causes crashes.

## Solution
Rebuild libheif with the updated configuration that has `WITH_X265=OFF`.

## Prerequisites
- Android NDK (version 28.1.13356709 or compatible)
- CMake (3.22.1 or compatible) 
- Ninja

## Build Steps

1. **Set up environment** (tools are in Android SDK):
   ```bash
   export PATH="$HOME/Library/Android/sdk/cmake/3.22.1/bin:$PATH"
   export NDK_PATH="$HOME/Library/Android/sdk/ndk/28.1.13356709"
   export NDK=$NDK_PATH
   ```

2. **Build all dependencies and libheif**:
   ```bash
   bash build_all.sh
   ```
   
   This will build (in order):
   - dav1d (AV1 decoder)
   - de265 (HEVC decoder)
   - yuv (YUV conversion)
   - sharpyuv (Sharp YUV conversion)
   - libheif (HEIF container, **without x265**)

3. **Rebuild Android project**:
   ```bash
   ./gradlew clean assembleDebug
   ```

## Expected Result
After rebuilding, `libheif.so` will no longer have the `libx265.so` dependency, and the app should run without the `UnsatisfiedLinkError`.

## Note
The build may take 10-30 minutes depending on your system. All build scripts have been updated to use the correct paths for your system.
