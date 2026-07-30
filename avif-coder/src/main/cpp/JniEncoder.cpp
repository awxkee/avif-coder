/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 15/9/2023
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <jni.h>
#include <string>
#include "android/bitmap.h"
#include <vector>
#include "JniException.h"
#include "SizeScaler.h"
#include <android/log.h>
#include <android/data_space.h>
#include <sys/system_properties.h>
#include "colorspace/colorspace.h"
#include <cmath>
#include <limits>
#include "imagebits/RgbaF16bitToNBitU16.h"
#include "imagebits/Rgb1010102.h"
#include "imagebits/RGBAlpha.h"
#include "imagebits/Rgb565.h"
#include "imagebits/CopyUnalignedRGBA.h"
#include "avif/avif.h"
#include "avif/avif_cxx.h"
#include <libyuv.h>
#include "AvifDecoderController.h"
#include "avifweaver.h"

using namespace std;

enum AvifQualityMode {
  AVIF_LOSSY_MODE = 1,
  AVIF_LOSELESS_MODE = 2
};

enum AvifChromaSubsampling {
  AVIF_CHROMA_AUTO,
  AVIF_CHROMA_YUV_420,
  AVIF_CHROMA_YUV_422,
  AVIF_CHROMA_YUV_444,
  AVIF_CHROMA_YUV_400,
  AVIF_CHROMA_LOSELESS
};

namespace {

bool readAvifEncodingOptions(JNIEnv *env,
                             jobject javaOptions,
                             jint dataSpace,
                             AvifEncodingOptions *options,
                             bool *useAv2) {
  if (javaOptions == nullptr) {
    std::string exception = "AVIF encoding options must not be null";
    throwException(env, exception);
    return false;
  }

  jclass optionsClass = env->GetObjectClass(javaOptions);
  if (optionsClass == nullptr) {
    return false;
  }

  jmethodID qualityMethod = env->GetMethodID(optionsClass, "getQualityValue", "()I");
  if (qualityMethod == nullptr) {
    env->DeleteLocalRef(optionsClass);
    return false;
  }
  jmethodID losslessMethod = env->GetMethodID(optionsClass, "isLossless", "()Z");
  if (losslessMethod == nullptr) {
    env->DeleteLocalRef(optionsClass);
    return false;
  }
  jmethodID chromaMethod = env->GetMethodID(
      optionsClass, "getChromaSubsamplingValue", "()I"
  );
  if (chromaMethod == nullptr) {
    env->DeleteLocalRef(optionsClass);
    return false;
  }
  jmethodID av2Method = env->GetMethodID(optionsClass, "useAv2", "()Z");
  if (av2Method == nullptr) {
    env->DeleteLocalRef(optionsClass);
    return false;
  }
  jmethodID speedMethod = env->GetMethodID(optionsClass, "getSpeedValue", "()I");
  if (speedMethod == nullptr) {
    env->DeleteLocalRef(optionsClass);
    return false;
  }
  jmethodID screenContentMethod = env->GetMethodID(
      optionsClass, "getScreenContentCoding", "()Z"
  );
  if (screenContentMethod == nullptr) {
    env->DeleteLocalRef(optionsClass);
    return false;
  }
  env->DeleteLocalRef(optionsClass);

  options->color_space = dataSpace;
  options->quality = env->CallIntMethod(javaOptions, qualityMethod);
  if (env->ExceptionCheck()) {
    return false;
  }
  options->lossless = env->CallBooleanMethod(javaOptions, losslessMethod) == JNI_TRUE;
  if (env->ExceptionCheck()) {
    return false;
  }
  options->chroma_subsampling_code = env->CallIntMethod(javaOptions, chromaMethod);
  if (env->ExceptionCheck()) {
    return false;
  }
  *useAv2 = env->CallBooleanMethod(javaOptions, av2Method) == JNI_TRUE;
  if (env->ExceptionCheck()) {
    return false;
  }
  jint speed = env->CallIntMethod(javaOptions, speedMethod);
  if (env->ExceptionCheck()) {
    return false;
  }

  options->speed = AvEncodingSpeed::Fast;
  if (speed == 1) {
    options->speed = AvEncodingSpeed::Medium;
  } else if (speed == 2) {
    options->speed = AvEncodingSpeed::Slow;
  }
  options->screen_content_coding =
      env->CallBooleanMethod(javaOptions, screenContentMethod) == JNI_TRUE;
  if (env->ExceptionCheck()) {
    return false;
  }
  return true;
}

bool readHevcEncodingOptions(JNIEnv *env,
                             jobject javaOptions,
                             jint dataSpace,
                             HevcEncodingOptions *options) {
  if (javaOptions == nullptr) {
    std::string exception = "HEVC encoding options must not be null";
    throwException(env, exception);
    return false;
  }

  jclass optionsClass = env->GetObjectClass(javaOptions);
  if (optionsClass == nullptr) {
    return false;
  }

  jmethodID qualityMethod = env->GetMethodID(optionsClass, "getQualityValue", "()I");
  if (qualityMethod == nullptr) {
    env->DeleteLocalRef(optionsClass);
    return false;
  }
  jmethodID losslessMethod = env->GetMethodID(optionsClass, "isLossless", "()Z");
  if (losslessMethod == nullptr) {
    env->DeleteLocalRef(optionsClass);
    return false;
  }
  jmethodID chromaMethod = env->GetMethodID(
      optionsClass, "getChromaSubsamplingValue", "()I"
  );
  if (chromaMethod == nullptr) {
    env->DeleteLocalRef(optionsClass);
    return false;
  }
  jmethodID speedMethod = env->GetMethodID(optionsClass, "getSpeedValue", "()I");
  if (speedMethod == nullptr) {
    env->DeleteLocalRef(optionsClass);
    return false;
  }
  jmethodID screenContentMethod = env->GetMethodID(
      optionsClass, "getScreenContentCoding", "()Z"
  );
  if (screenContentMethod == nullptr) {
    env->DeleteLocalRef(optionsClass);
    return false;
  }
  jmethodID rdpcmMethod = env->GetMethodID(optionsClass, "getRdpcm", "()Z");
  env->DeleteLocalRef(optionsClass);

  if (rdpcmMethod == nullptr) {
    return false;
  }

  options->color_space = dataSpace;
  options->quality = env->CallIntMethod(javaOptions, qualityMethod);
  if (env->ExceptionCheck()) {
    return false;
  }
  options->chroma_subsampling_code = env->CallIntMethod(javaOptions, chromaMethod);
  if (env->ExceptionCheck()) {
    return false;
  }
  options->lossless = env->CallBooleanMethod(javaOptions, losslessMethod) == JNI_TRUE;
  if (env->ExceptionCheck()) {
    return false;
  }
  options->speed = env->CallIntMethod(javaOptions, speedMethod);
  if (env->ExceptionCheck()) {
    return false;
  }
  options->screen_content_coding =
      env->CallBooleanMethod(javaOptions, screenContentMethod) == JNI_TRUE;
  if (env->ExceptionCheck()) {
    return false;
  }
  options->rdpcm = env->CallBooleanMethod(javaOptions, rdpcmMethod) == JNI_TRUE;
  return !env->ExceptionCheck();
}

}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_radzivon_bartoshyk_avif_coder_Coder_encodeAvifImpl(JNIEnv *env,
                                                            jobject thiz,
                                                            jobject bitmap,
                                                            jobject exif,
                                                            jint dataSpace,
                                                            jobject javaOptions) {
  try {
    AvifEncodingOptions options{};
    bool useAv2 = false;
    if (!readAvifEncodingOptions(env, javaOptions, dataSpace, &options, &useAv2)) {
      return static_cast<jbyteArray>(nullptr);
    }
    if (useAv2) {
      return encode_avif_av2_file(
          env, bitmap, exif, options
      );
    } else {
      return encode_avif_av1_file(
          env, bitmap, exif, options
      );
    }
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to encode this image";
    throwException(env, exception);
    return static_cast<jbyteArray>(nullptr);
  }
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_radzivon_bartoshyk_avif_coder_Coder_encodeHeicImpl(JNIEnv *env,
                                                            jobject thiz,
                                                            jobject bitmap,
                                                            jobject exif,
                                                            jint dataSpace,
                                                            jobject javaOptions) {
  HevcEncodingOptions options{};
  if (!readHevcEncodingOptions(env, javaOptions, dataSpace, &options)) {
    return static_cast<jbyteArray>(nullptr);
  }
  return encode_heic_file(env, bitmap, exif, options);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_radzivon_bartoshyk_avif_coder_Coder_isHeifImageImpl(JNIEnv *env, jobject thiz,
                                                             jbyteArray byte_array) {
  try {
    auto totalLength = std::min(env->GetArrayLength(byte_array), 65535);
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(byte_array, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));
    return is_heic_image(srcBuffer.data(), srcBuffer.size());
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to check this image";
    throwException(env, exception);
    return false;
  }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_radzivon_bartoshyk_avif_coder_Coder_isAvifImageImpl(JNIEnv *env, jobject thiz,
                                                             jbyteArray byte_array) {
  try {
    auto totalLength = std::min(env->GetArrayLength(byte_array), 65535);
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(byte_array, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));
    return is_avif_image(srcBuffer.data(), srcBuffer.size());
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to check this image";
    throwException(env, exception);
    return false;
  }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_radzivon_bartoshyk_avif_coder_Coder_isSupportedImageImpl(JNIEnv *env, jobject thiz,
                                                                  jbyteArray byte_array) {
  try {
    auto totalLength = std::min(env->GetArrayLength(byte_array), 65535);
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(byte_array, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));
    return container_recognisance(srcBuffer.data(), srcBuffer.size()) != ImageContainer::Unknown;
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to check this image";
    throwException(env, exception);
    return false;
  }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_Coder_getSizeImpl(JNIEnv *env, jobject thiz,
                                                         jbyteArray byteArray) {
  try {
    auto totalLength = env->GetArrayLength(byteArray);
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(byteArray, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));

    auto containerType = container_recognisance(srcBuffer.data(), srcBuffer.size());

    if (containerType == ImageContainer::Avif) {
      AvifImageSize size = AvifDecoderController::getImageSize(srcBuffer.data(), srcBuffer.size());
      jclass sizeClass = env->FindClass("android/util/Size");
      jmethodID methodID = env->GetMethodID(sizeClass, "<init>", "(II)V");
      auto sizeObject = env->NewObject(sizeClass,
                                       methodID,
                                       static_cast<jint >(size.width),
                                       static_cast<jint>(size.height));
      return sizeObject;
    } else if (containerType == ImageContainer::Av2) {
      auto result = read_av2_file_info(srcBuffer.data(), srcBuffer.size());
      if (!result.supported_image) {
        std::string exception = "Reading a AV2 image has failed";
        throwException(env, exception);
        return static_cast<jobject>(nullptr);
      }

      jclass sizeClass = env->FindClass("android/util/Size");
      jmethodID methodID = env->GetMethodID(sizeClass, "<init>", "(II)V");
      auto sizeObject = env->NewObject(sizeClass,
                                       methodID,
                                       static_cast<jint>(result.width),
                                       static_cast<jint>(result.height));
      return sizeObject;
    }

    auto result = read_heic_file_info(srcBuffer.data(), srcBuffer.size());
    if (!result.supported_image) {
      std::string exception = "Reading a HEIC image has failed";
      throwException(env, exception);
      return static_cast<jobject>(nullptr);
    }

    jclass sizeClass = env->FindClass("android/util/Size");
    jmethodID methodID = env->GetMethodID(sizeClass, "<init>", "(II)V");
    auto sizeObject = env->NewObject(sizeClass,
                                     methodID,
                                     static_cast<jint>(result.width),
                                     static_cast<jint>(result.height));
    return sizeObject;
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to load size of this image";
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  } catch (std::runtime_error &err) {
    std::string exception(err.what());
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  }
}


extern "C"
JNIEXPORT jboolean JNICALL
Java_com_radzivon_bartoshyk_avif_coder_Coder_isSupportedImageImplBB(JNIEnv *env, jobject thiz,
                                                                    jobject byteBuffer) {
  try {
    auto bufferAddress = reinterpret_cast<const uint8_t *>(env->GetDirectBufferAddress(byteBuffer));
    auto length = (size_t) env->GetDirectBufferCapacity(byteBuffer);
    if (!bufferAddress || length <= 0) {
      std::string errorString = "Only direct byte buffers are supported";
      throwException(env, errorString);
      return (jboolean) false;
    }
    return container_recognisance(bufferAddress, length) != ImageContainer::Unknown;
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to check this image";
    throwException(env, exception);
    return false;
  }
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_radzivon_bartoshyk_avif_coder_Coder_detectContainerImpl(JNIEnv *env, jobject thiz,
                                                                 jobject byteBuffer) {
  try {
    auto bufferAddress = reinterpret_cast<const uint8_t *>(env->GetDirectBufferAddress(byteBuffer));
    auto length = (size_t) env->GetDirectBufferCapacity(byteBuffer);
    if (!bufferAddress || length <= 0) {
      std::string errorString = "Only direct byte buffers are supported";
      throwException(env, errorString);
      return (jboolean) false;
    }
    switch (container_recognisance(bufferAddress, length)) {
      case ImageContainer::Unknown:return -1;
      case ImageContainer::Heic:return 1;
      case ImageContainer::Avif:return 2;
      case ImageContainer::Av2:return 3;
      case ImageContainer::Vvc:return 4;
    }
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to check this image";
    throwException(env, exception);
    return false;
  }
}
