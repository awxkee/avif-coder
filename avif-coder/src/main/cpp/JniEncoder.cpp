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


extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_radzivon_bartoshyk_avif_coder_Coder_encodeAvifImpl(JNIEnv *env,
                                                            jobject thiz,
                                                            jobject bitmap,
                                                            jobject exif,
                                                            jint quality,
                                                            jint dataSpace,
                                                            jboolean lossless,
                                                            jint chromaSubsampling) {
  try {
    return encode_avif_av1_file(
        env, bitmap, exif, dataSpace, quality, lossless, chromaSubsampling
    );
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
                                                            jint quality,
                                                            jint dataSpace,
                                                            jint chroma,
                                                            jboolean lossless) {
  return encode_heic_file(
      env, bitmap, exif, dataSpace, quality, chroma, lossless
  );
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_radzivon_bartoshyk_avif_coder_Coder_isHeifImageImpl(JNIEnv *env, jobject thiz,
                                                             jbyteArray byte_array) {
  try {
    auto totalLength = env->GetArrayLength(byte_array);
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
    auto totalLength = env->GetArrayLength(byte_array);
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
    auto totalLength = env->GetArrayLength(byte_array);
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(byte_array, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));
    return is_heic_image(srcBuffer.data(), srcBuffer.size())
        || is_avif_image(srcBuffer.data(), srcBuffer.size());
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

    if (is_avif_image(srcBuffer.data(), srcBuffer.size())) {
      AvifImageSize size = AvifDecoderController::getImageSize(srcBuffer.data(), srcBuffer.size());
      jclass sizeClass = env->FindClass("android/util/Size");
      jmethodID methodID = env->GetMethodID(sizeClass, "<init>", "(II)V");
      auto sizeObject = env->NewObject(sizeClass,
                                       methodID,
                                       static_cast<jint >(size.width),
                                       static_cast<jint>(size.height));
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
    auto sizeObject = env->NewObject(sizeClass, methodID, result.width, result.height);
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
    auto bufferAddress = reinterpret_cast<uint8_t *>(env->GetDirectBufferAddress(byteBuffer));
    int length = (int) env->GetDirectBufferCapacity(byteBuffer);
    if (!bufferAddress || length <= 0) {
      std::string errorString = "Only direct byte buffers are supported";
      throwException(env, errorString);
      return (jboolean) false;
    }
    std::vector<uint8_t> srcBuffer(length);
    std::copy(bufferAddress, bufferAddress + length, srcBuffer.begin());
    return is_heic_image(srcBuffer.data(), srcBuffer.size())
        || is_avif_image(srcBuffer.data(), srcBuffer.size());
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to check this image";
    throwException(env, exception);
    return false;
  }
}