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
#include "AvifDecoderController.h"
#include "ReformatBitmap.h"
#include "JniBitmap.h"
#include <dlfcn.h>
#include "avifweaver.h"

using namespace std;

jobject decodeImplementationNative(JNIEnv *env, jobject thiz,
                                   std::vector<uint8_t> &srcBuffer, jint scaledWidth,
                                   jint scaledHeight, jint clrConfig, jint javaScaleMode,
                                   jint scalingQuality) {
  PreferredColorConfig preferredColorConfig;
  ScaleMode scaleMode;

  if (!checkDecodePreconditions(env, clrConfig, &preferredColorConfig, javaScaleMode,
                                &scaleMode)) {
    string exception = "Can't retrieve basic values";
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  }

  try {

    AvifImageFrame frame;

    if (is_avif_image(srcBuffer.data(), srcBuffer.size())) {
      AvifDecoderController avifController;
      avifController.attachBuffer(srcBuffer.data(), srcBuffer.size());
      frame = avifController.getFrame(0,
                                      scaledWidth,
                                      scaledHeight,
                                      preferredColorConfig,
                                      scaleMode,
                                      scalingQuality);
    } else {
      WeaveScaleMode mScaleMode = WeaveScaleMode::ScaleToFill;
      if (scaleMode == 1) {
        mScaleMode = WeaveScaleMode::ScaleToFit;
      } else if (scaleMode == 3) {
        mScaleMode = WeaveScaleMode::JustResize;
      }
      auto mConfig = WeaverPreferredColorConfig::Default;
      if (clrConfig == 2) {
        mConfig = WeaverPreferredColorConfig::Rgba8888;
      } else if (clrConfig == 3) {
        mConfig = WeaverPreferredColorConfig::RgbaF16;
      } else if (clrConfig == 4) {
        mConfig = WeaverPreferredColorConfig::Rgb565;
      } else if (clrConfig == 5) {
        mConfig = WeaverPreferredColorConfig::Rgba1010102;
      } else if (clrConfig == 6) {
        mConfig = WeaverPreferredColorConfig::Hardware;
      }

      return decode_heic_file(env,
                              srcBuffer.data(),
                              srcBuffer.size(),
                              scaledWidth,
                              scaledHeight,
                              mScaleMode, mConfig);
    }

    // The controller owns its own compressed-input copy and has already been
    // destroyed here. Release the JNI input before color conversion and the
    // hardware-buffer upload so large files do not add avoidable native-memory
    // pressure at the gralloc lock boundary.
    std::vector<uint8_t>().swap(srcBuffer);

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
    if (env->ExceptionCheck()) {
      return static_cast<jobject>(nullptr);
    }

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
Java_com_radzivon_bartoshyk_avif_coder_Coder_decodeImpl(JNIEnv *env,
                                                        jobject thiz,
                                                        jbyteArray byte_array,
                                                        jint scaledWidth,
                                                        jint scaledHeight,
                                                        jint clrConfig,
                                                        jint scaleMode,
                                                        jint scaleQuality) {
  try {
    auto totalLength = env->GetArrayLength(byte_array);
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(byte_array, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));

    auto containerType = container_recognisance(srcBuffer.data(), srcBuffer.size());

    WeaveScaleMode mScaleMode = WeaveScaleMode::ScaleToFill;
    if (scaleMode == 1) {
      mScaleMode = WeaveScaleMode::ScaleToFit;
    } else if (scaleMode == 3) {
      mScaleMode = WeaveScaleMode::JustResize;
    }
    auto mConfig = WeaverPreferredColorConfig::Default;
    if (clrConfig == 2) {
      mConfig = WeaverPreferredColorConfig::Rgba8888;
    } else if (clrConfig == 3) {
      mConfig = WeaverPreferredColorConfig::RgbaF16;
    } else if (clrConfig == 4) {
      mConfig = WeaverPreferredColorConfig::Rgb565;
    } else if (clrConfig == 5) {
      mConfig = WeaverPreferredColorConfig::Rgba1010102;
    } else if (clrConfig == 6) {
      mConfig = WeaverPreferredColorConfig::Hardware;
    }

    if (containerType == ImageContainer::Heic) {
      return decode_heic_file(env,
                              srcBuffer.data(),
                              srcBuffer.size(),
                              scaledWidth,
                              scaledHeight,
                              mScaleMode, mConfig);
    } else if (containerType == ImageContainer::Av2) {
      return decode_av2_file(env,
                             srcBuffer.data(),
                             srcBuffer.size(),
                             scaledWidth,
                             scaledHeight,
                             mScaleMode, mConfig);
    }
    return decodeImplementationNative(env, thiz, srcBuffer,
                                      scaledWidth, scaledHeight,
                                      clrConfig, scaleMode,
                                      scaleQuality);
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to decode this image";
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  }
}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_Coder_decodeByteBufferImpl(JNIEnv *env,
                                                                  jobject thiz,
                                                                  jobject byteBuffer,
                                                                  jint scaledWidth,
                                                                  jint scaledHeight,
                                                                  jint clrConfig,
                                                                  jint scaleMode,
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
    auto containerType = container_recognisance(srcBuffer.data(), srcBuffer.size());

    WeaveScaleMode mScaleMode = WeaveScaleMode::ScaleToFill;
    if (scaleMode == 1) {
      mScaleMode = WeaveScaleMode::ScaleToFit;
    } else if (scaleMode == 3) {
      mScaleMode = WeaveScaleMode::JustResize;
    }
    auto mConfig = WeaverPreferredColorConfig::Default;
    if (clrConfig == 2) {
      mConfig = WeaverPreferredColorConfig::Rgba8888;
    } else if (clrConfig == 3) {
      mConfig = WeaverPreferredColorConfig::RgbaF16;
    } else if (clrConfig == 4) {
      mConfig = WeaverPreferredColorConfig::Rgb565;
    } else if (clrConfig == 5) {
      mConfig = WeaverPreferredColorConfig::Rgba1010102;
    } else if (clrConfig == 6) {
      mConfig = WeaverPreferredColorConfig::Hardware;
    }

    if (containerType == ImageContainer::Heic) {
      return decode_heic_file(env,
                              srcBuffer.data(),
                              srcBuffer.size(),
                              scaledWidth,
                              scaledHeight,
                              mScaleMode, mConfig);
    } else if (containerType == ImageContainer::Av2) {
      return decode_av2_file(env,
                             srcBuffer.data(),
                             srcBuffer.size(),
                             scaledWidth,
                             scaledHeight,
                             mScaleMode, mConfig);
    }
    return decodeImplementationNative(env, thiz, srcBuffer,
                                      scaledWidth, scaledHeight,
                                      clrConfig, scaleMode, scalingQuality);
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to decode this image";
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  }
}