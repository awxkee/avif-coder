/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 13/10/2024
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
#include "AvifDecoderController.h"
#include "JniException.h"
#include "aligned_allocator.h"
#include "JniBitmap.h"
#include "ReformatBitmap.h"

extern "C"
JNIEXPORT void JNICALL
Java_com_radzivon_bartoshyk_avif_coder_AvifAnimatedDecoder_destroy(JNIEnv *env,
                                                                   jobject thiz,
                                                                   jlong ptr) {
  auto controller = reinterpret_cast<AvifDecoderController *>(ptr);
  delete controller;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_radzivon_bartoshyk_avif_coder_AvifAnimatedDecoder_createControllerFromByteArray(JNIEnv *env,
                                                                                         jobject thiz,
                                                                                         jbyteArray byteArray) {
  try {
    auto totalLength = env->GetArrayLength(byteArray);
    aligned_uint8_vector srcBuffer(totalLength);
    env->GetByteArrayRegion(byteArray, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));
    auto controller = new AvifDecoderController(srcBuffer.data(), srcBuffer.size());
    return reinterpret_cast<jlong>(controller);
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to decode this image";
    throwException(env, exception);
    return static_cast<jlong>(-1);
  } catch (std::runtime_error &err) {
    std::string exception(err.what());
    throwException(env, exception);
    return static_cast<jlong>(-1);
  }
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_radzivon_bartoshyk_avif_coder_AvifAnimatedDecoder_createControllerFromByteBuffer(JNIEnv *env,
                                                                                          jobject thiz,
                                                                                          jobject byteBuffer) {
  try {
    auto bufferAddress = reinterpret_cast<uint8_t *>(env->GetDirectBufferAddress(byteBuffer));
    int length = (int) env->GetDirectBufferCapacity(byteBuffer);
    if (!bufferAddress || length <= 0) {
      std::string errorString = "Only direct byte buffers are supported";
      throwException(env, errorString);
      return static_cast<jlong>(-1);
    }
    aligned_uint8_vector srcBuffer(length);
    std::copy(bufferAddress, bufferAddress + length, srcBuffer.begin());
    auto controller = new AvifDecoderController(srcBuffer.data(), srcBuffer.size());
    return reinterpret_cast<jlong>(controller);
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to decode this image";
    throwException(env, exception);
    return static_cast<jlong>(-1);
  } catch (std::runtime_error &err) {
    std::string exception(err.what());
    throwException(env, exception);
    return static_cast<jlong>(-1);
  }
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_radzivon_bartoshyk_avif_coder_AvifAnimatedDecoder_getFramesCount(JNIEnv *env,
                                                                          jobject thiz,
                                                                          jlong ptr) {
  try {
    auto controller = reinterpret_cast<AvifDecoderController *>(ptr);
    return static_cast<jint >(controller->getFramesCount());
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to decode this image";
    throwException(env, exception);
    return static_cast<jlong>(-1);
  } catch (std::runtime_error &err) {
    std::string exception(err.what());
    throwException(env, exception);
    return static_cast<jlong>(-1);
  }
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_radzivon_bartoshyk_avif_coder_AvifAnimatedDecoder_getLoopsCountImpl(JNIEnv *env,
                                                                             jobject thiz,
                                                                             jlong ptr) {
  try {
    auto controller = reinterpret_cast<AvifDecoderController *>(ptr);
    return static_cast<jint >(controller->getLoopsCount());
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to decode this image";
    throwException(env, exception);
    return static_cast<jlong>(-1);
  } catch (std::runtime_error &err) {
    std::string exception(err.what());
    throwException(env, exception);
    return static_cast<jlong>(-1);
  }
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_radzivon_bartoshyk_avif_coder_AvifAnimatedDecoder_getTotalDurationImpl(JNIEnv *env,
                                                                                jobject thiz,
                                                                                jlong ptr) {
  try {
    auto controller = reinterpret_cast<AvifDecoderController *>(ptr);
    return static_cast<jint >(controller->getTotalDuration());
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to decode this image";
    throwException(env, exception);
    return static_cast<jlong>(-1);
  } catch (std::runtime_error &err) {
    std::string exception(err.what());
    throwException(env, exception);
    return static_cast<jlong>(-1);
  }
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_radzivon_bartoshyk_avif_coder_AvifAnimatedDecoder_getFrameDurationImpl(JNIEnv *env,
                                                                                jobject thiz,
                                                                                jlong ptr,
                                                                                jint frame) {
  try {
    auto controller = reinterpret_cast<AvifDecoderController *>(ptr);
    return static_cast<jint >(controller->getFrameDuration(static_cast<uint32_t>(frame)));
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to decode this image";
    throwException(env, exception);
    return static_cast<jlong>(-1);
  } catch (std::runtime_error &err) {
    std::string exception(err.what());
    throwException(env, exception);
    return static_cast<jlong>(-1);
  }
}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_AvifAnimatedDecoder_getFrameImpl(JNIEnv *env,
                                                                        jobject thiz,
                                                                        jlong ptr,
                                                                        jint frameIndex,
                                                                        jint scaledWidth,
                                                                        jint scaledHeight,
                                                                        jint javaColorSpace,
                                                                        jint javaScaleMode,
                                                                        jint scaleQuality,
                                                                        jint javaToneMapper) {
  try {
    PreferredColorConfig preferredColorConfig;
    ScaleMode scaleMode;
    if (!checkDecodePreconditions(env, javaColorSpace, &preferredColorConfig, javaScaleMode,
                                  &scaleMode)) {
      std::string exception = "Can't retrieve basic values";
      throwException(env, exception);
      return static_cast<jobject>(nullptr);
    }

    auto controller = reinterpret_cast<AvifDecoderController *>(ptr);
    auto frame = controller->getFrame(frameIndex,
                                      scaledWidth,
                                      scaledHeight,
                                      preferredColorConfig,
                                      scaleMode,
                                      scaleQuality);

    int osVersion = androidOSVersion();

    bool useBitmapHalf16Floats = false;

    if (frame.is16Bit && osVersion >= 26) {
      useBitmapHalf16Floats = true;
    }

    std::string imageConfig = useBitmapHalf16Floats ? "RGBA_F16" : "ARGB_8888";

    jobject hwBuffer = nullptr;

    uint32_t stride = frame.width * 4 * (frame.is16Bit ? sizeof(uint16_t) : sizeof(uint8_t));

    coder::ReformatColorConfig(env, ref(frame.store), ref(imageConfig), preferredColorConfig,
                               frame.bitDepth, frame.width,
                               frame.height, &stride, &useBitmapHalf16Floats, &hwBuffer,
                               false, frame.hasAlpha);

    return createBitmap(env, ref(frame.store), imageConfig, stride, frame.width, frame.height,
                        useBitmapHalf16Floats, hwBuffer);
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to decode this image";
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  } catch (std::runtime_error &err) {
    std::string exception(err.what());
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_AvifAnimatedDecoder_getSizeImpl(JNIEnv *env,
                                                                       jobject thiz,
                                                                       jlong ptr) {
  try {
    auto controller = reinterpret_cast<AvifDecoderController *>(ptr);
    auto size = controller->getImageSize();
    jclass sizeClass = env->FindClass("android/util/Size");
    jmethodID methodID = env->GetMethodID(sizeClass, "<init>", "(II)V");
    auto sizeObject = env->NewObject(sizeClass,
                                     methodID,
                                     static_cast<int>(size.width),
                                     static_cast<int>(size.height));
    return sizeObject;
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to decode this image";
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  } catch (std::runtime_error &err) {
    std::string exception(err.what());
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  }
}