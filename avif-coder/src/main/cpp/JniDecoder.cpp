//
// Created by Radzivon Bartoshyk on 15/09/2023.
//
#include <jni.h>
#include <string>
#include "android/bitmap.h"
#include <vector>
#include "JniException.h"
#include <android/log.h>
#include <android/data_space.h>
#include <algorithm>
#include <limits>
#include "thread"
#include <android/asset_manager_jni.h>
#include "definitions.h"
#include <Support.h>
#include "HeifImageDecoder.h"
#include "AvifDecoderController.h"
#include "ReformatBitmap.h"
#include "JniBitmap.h"
#include <dlfcn.h>

using namespace std;

/**
 * Internal function to decode an image from a buffer.
 * 
 * @param env JNI environment
 * @param thiz Java object reference
 * @param srcBuffer The image data buffer
 * @param scaledWidth Target width (0 means no scaling)
 * @param scaledHeight Target height (0 means no scaling)
 * @param javaColorSpace Preferred color configuration
 * @param javaScaleMode Scaling mode (FIT or FILL)
 * @param scalingQuality Quality of scaling
 * @return Decoded Bitmap object, or nullptr on error
 */
jobject decodeImplementationNative(JNIEnv *env, jobject thiz,
                                   std::vector<uint8_t> &srcBuffer, jint scaledWidth,
                                   jint scaledHeight, jint javaColorSpace, jint javaScaleMode,
                                   jint scalingQuality) {
  PreferredColorConfig preferredColorConfig;
  ScaleMode scaleMode;

  if (!checkDecodePreconditions(env, javaColorSpace, &preferredColorConfig, javaScaleMode,
                                &scaleMode)) {
    string exception = "Can't retrieve basic values";
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  }

  try {

    std::string mimeType = HeifImageDecoder::getImageType(srcBuffer);
    AvifImageFrame frame;

    if (mimeType == "image/avif" || mimeType == "image/avif-sequence") {
      AvifDecoderController avifController;
      avifController.attachBuffer(srcBuffer.data(), srcBuffer.size());
      frame = avifController.getFrame(0,
                                      scaledWidth,
                                      scaledHeight,
                                      preferredColorConfig,
                                      scaleMode,
                                      scalingQuality);
    } else {
      HeifImageDecoder heifDecoder;
      frame = heifDecoder.getFrame(srcBuffer,
                                   scaledWidth,
                                   scaledHeight,
                                   preferredColorConfig,
                                   scaleMode,
                                   scalingQuality);
    }

    int osVersion = androidOSVersion();

    bool useBitmapHalf16Floats = false;

    if (frame.is16Bit && osVersion >= 26) {
      useBitmapHalf16Floats = true;
    }

    string imageConfig = useBitmapHalf16Floats ? "RGBA_F16" : "ARGB_8888";

    jobject hwBuffer = nullptr;

    uint32_t stride = frame.width * 4 * (frame.is16Bit ? sizeof(uint16_t) : sizeof(uint8_t));

    coder::ReformatColorConfig(env, ref(frame.store), ref(imageConfig), preferredColorConfig,
                               frame.bitDepth, frame.width,
                               frame.height, &stride, &useBitmapHalf16Floats, &hwBuffer,
                               false, frame.hasAlpha);

    return createBitmap(env, ref(frame.store), imageConfig, stride, frame.width, frame.height,
                        useBitmapHalf16Floats, hwBuffer);
  } catch (std::runtime_error &err) {
    string exception(err.what());
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  }
}

/**
 * JNI function to decode an AVIF or HEIF/HEIC image from a ByteArray.
 * 
 * @param env JNI environment
 * @param thiz Java object reference
 * @param byte_array The image data as a ByteArray
 * @param scaledWidth Target width (0 means no scaling)
 * @param scaledHeight Target height (0 means no scaling)
 * @param javaColorspace Preferred color configuration
 * @param scaleMode Scaling mode (FIT or FILL)
 * @param scaleQuality Quality of scaling
 * @return Decoded Bitmap object, or nullptr on error
 */
extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_decodeImpl(JNIEnv *env,
                                                            jobject thiz,
                                                            jbyteArray byte_array,
                                                            jint scaledWidth,
                                                            jint scaledHeight,
                                                            jint javaColorspace,
                                                            jint scaleMode,
                                                            jint scaleQuality) {
  try {
    if (!byte_array) {
      std::string exception = "ByteArray cannot be null";
      throwException(env, exception);
      return static_cast<jobject>(nullptr);
    }
    auto totalLength = env->GetArrayLength(byte_array);
    if (totalLength <= 0) {
      std::string exception = "Buffer size must be greater than zero";
      throwException(env, exception);
      return static_cast<jobject>(nullptr);
    }
    if (scaledWidth < 0 || scaledHeight < 0) {
      std::string exception = "Scaled dimensions must be non-negative";
      throwException(env, exception);
      return static_cast<jobject>(nullptr);
    }
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(byte_array, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));
    return decodeImplementationNative(env, thiz, srcBuffer,
                                      scaledWidth, scaledHeight,
                                      javaColorspace, scaleMode,
                                      scaleQuality);
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to decode this image";
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  }
}
/**
 * JNI function to decode an AVIF or HEIF/HEIC image from a ByteBuffer.
 * 
 * @param env JNI environment
 * @param thiz Java object reference
 * @param byteBuffer The image data as a direct ByteBuffer
 * @param scaledWidth Target width (0 means no scaling)
 * @param scaledHeight Target height (0 means no scaling)
 * @param clrConfig Preferred color configuration
 * @param scaleMode Scaling mode (FIT or FILL)
 * @param scalingQuality Quality of scaling
 * @return Decoded Bitmap object, or nullptr on error
 */
extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_decodeByteBufferImpl(JNIEnv *env,
                                                                      jobject thiz,
                                                                      jobject byteBuffer,
                                                                      jint scaledWidth,
                                                                      jint scaledHeight,
                                                                      jint clrConfig,
                                                                      jint scaleMode,
                                                                      jint scalingQuality) {
  try {
    if (!byteBuffer) {
      std::string errorString = "ByteBuffer cannot be null";
      throwException(env, errorString);
      return static_cast<jobject>(nullptr);
    }
    auto bufferAddress = reinterpret_cast<uint8_t *>(env->GetDirectBufferAddress(byteBuffer));
    int length = (int) env->GetDirectBufferCapacity(byteBuffer);
    if (!bufferAddress || length <= 0) {
      std::string errorString = "Only direct byte buffers are supported";
      throwException(env, errorString);
      return static_cast<jobject>(nullptr);
    }
    if (scaledWidth < 0 || scaledHeight < 0) {
      std::string exception = "Scaled dimensions must be non-negative";
      throwException(env, exception);
      return static_cast<jobject>(nullptr);
    }
    // Copy buffer to ensure memory safety and proper alignment.
    // The ByteBuffer may be freed by Java GC, and decoder functions require
    // a contiguous, aligned buffer that persists for the duration of decoding.
    std::vector<uint8_t> srcBuffer(length);
    std::copy(bufferAddress, bufferAddress + length, srcBuffer.begin());
    return decodeImplementationNative(env, thiz, srcBuffer,
                                      scaledWidth, scaledHeight,
                                      clrConfig, scaleMode, scalingQuality);
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to decode this image";
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  }
}

/**
 * JNI function to get the dimensions of an image without fully decoding it.
 * 
 * @param env JNI environment
 * @param thiz Java object reference
 * @param byte_array The image data as a ByteArray
 * @return Size object with width and height, or nullptr on error
 */
extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_getSizeImpl(JNIEnv *env,
                                                             jobject thiz,
                                                             jbyteArray byte_array) {
  try {
    if (!byte_array) {
      std::string exception = "ByteArray cannot be null";
      throwException(env, exception);
      return static_cast<jobject>(nullptr);
    }
    auto totalLength = env->GetArrayLength(byte_array);
    if (totalLength <= 0) {
      std::string exception = "Buffer size must be greater than zero";
      throwException(env, exception);
      return static_cast<jobject>(nullptr);
    }
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(byte_array, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));

    std::string mimeType = HeifImageDecoder::getImageType(srcBuffer);
    AvifImageSize size;

    if (mimeType == "image/avif" || mimeType == "image/avif-sequence") {
      size = AvifDecoderController::getImageSize(srcBuffer.data(), srcBuffer.size());
    } else {
      size = HeifImageDecoder::getImageSize(srcBuffer);
    }

    jclass sizeClass = env->FindClass("android/util/Size");
    if (!sizeClass) {
      std::string exception = "Can't find Size class";
      throwException(env, exception);
      return static_cast<jobject>(nullptr);
    }

    jmethodID methodID = env->GetMethodID(sizeClass, "<init>", "(II)V");
    if (!methodID) {
      std::string exception = "Can't find Size constructor";
      throwException(env, exception);
      return static_cast<jobject>(nullptr);
    }

    jobject sizeObject = env->NewObject(sizeClass,
                                        methodID,
                                        static_cast<jint>(size.width),
                                        static_cast<jint>(size.height));
    return sizeObject;
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to get image size";
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  } catch (std::runtime_error &err) {
    std::string exception(err.what());
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  }
}