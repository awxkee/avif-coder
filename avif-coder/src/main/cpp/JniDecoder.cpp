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

jobject decodeImplementationNative(JNIEnv *env, jobject thiz,
                                   std::vector<uint8_t> &srcBuffer, jint scaledWidth,
                                   jint scaledHeight, jint javaColorSpace, jint javaScaleMode,
                                   jint javaToneMapper, jint scalingQuality) {
  PreferredColorConfig preferredColorConfig;
  ScaleMode scaleMode;
  CurveToneMapper toneMapper;

  if (!checkDecodePreconditions(env, javaColorSpace, &preferredColorConfig, javaScaleMode,
                                &scaleMode, javaToneMapper, &toneMapper)) {
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
                                      toneMapper,
                                      scalingQuality);
    } else {
      HeifImageDecoder heifDecoder;
      frame = heifDecoder.getFrame(srcBuffer,
                                   scaledWidth,
                                   scaledHeight,
                                   preferredColorConfig,
                                   scaleMode,
                                   toneMapper,
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

extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_decodeImpl(JNIEnv *env,
                                                            jobject thiz,
                                                            jbyteArray byte_array,
                                                            jint scaledWidth,
                                                            jint scaledHeight,
                                                            jint javaColorspace,
                                                            jint scaleMode,
                                                            jint javaToneMapper,
                                                            jint scaleQuality) {
  try {
    auto totalLength = env->GetArrayLength(byte_array);
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(byte_array, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));
    return decodeImplementationNative(env, thiz, srcBuffer,
                                      scaledWidth, scaledHeight,
                                      javaColorspace, scaleMode,
                                      javaToneMapper, scaleQuality);
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to decode this image";
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  }
}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_decodeByteBufferImpl(JNIEnv *env,
                                                                      jobject thiz,
                                                                      jobject byteBuffer,
                                                                      jint scaledWidth,
                                                                      jint scaledHeight,
                                                                      jint clrConfig,
                                                                      jint scaleMode,
                                                                      jint javaToneMapper,
                                                                      jint scalingQuality) {
  try {
    auto bufferAddress = reinterpret_cast<uint8_t *>(env->GetDirectBufferAddress(byteBuffer));
    int length = (int) env->GetDirectBufferCapacity(byteBuffer);
    if (!bufferAddress || length <= 0) {
      std::string errorString = "Only direct byte buffers are supported";
      throwException(env, errorString);
      return nullptr;
    }
    std::vector<uint8_t> srcBuffer(length);
    std::copy(bufferAddress, bufferAddress + length, srcBuffer.begin());
    return decodeImplementationNative(env, thiz, srcBuffer,
                                      scaledWidth, scaledHeight,
                                      clrConfig, scaleMode, javaToneMapper, scalingQuality);
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to decode this image";
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  }
}