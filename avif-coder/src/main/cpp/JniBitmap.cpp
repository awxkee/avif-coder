/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 16/9/2023
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

#include "JniBitmap.h"
#include <jni.h>
#include <vector>
#include "JniException.h"
#include <android/bitmap.h>
#include "imagebits/CopyUnalignedRGBA.h"

jobject
createBitmap(JNIEnv *env, aligned_uint8_vector &data, std::string &colorConfig, int stride,
             int imageWidth, int imageHeight, bool use16Floats, jobject hwBuffer) {
  if (colorConfig == "HARDWARE") {
    jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
    jmethodID createBitmapMethodID = env->GetStaticMethodID(bitmapClass,
                                                            "wrapHardwareBuffer",
                                                            "(Landroid/hardware/HardwareBuffer;Landroid/graphics/ColorSpace;)Landroid/graphics/Bitmap;");
    jobject emptyObject = nullptr;
    jobject bitmapObj = env->CallStaticObjectMethod(bitmapClass, createBitmapMethodID,
                                                    hwBuffer, emptyObject);
    return bitmapObj;
  }
  jclass bitmapConfig = env->FindClass("android/graphics/Bitmap$Config");
  jfieldID rgba8888FieldID = env->GetStaticFieldID(bitmapConfig, colorConfig.c_str(),
                                                   "Landroid/graphics/Bitmap$Config;");
  jobject rgba8888Obj = env->GetStaticObjectField(bitmapConfig, rgba8888FieldID);

  jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
  jmethodID createBitmapMethodID = env->GetStaticMethodID(bitmapClass,
                                                          "createBitmap",
                                                          "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
  jobject bitmapObj = env->CallStaticObjectMethod(bitmapClass, createBitmapMethodID,
                                                  imageWidth, imageHeight, rgba8888Obj);

  AndroidBitmapInfo info;
  if (AndroidBitmap_getInfo(env, bitmapObj, &info) < 0) {
    throwPixelsException(env);
    return static_cast<jbyteArray>(nullptr);
  }

  void *addr;
  if (AndroidBitmap_lockPixels(env, bitmapObj, &addr) != 0) {
    throwPixelsException(env);
    return static_cast<jobject>(nullptr);
  }

  if (colorConfig == "RGB_565") {
    coder::CopyUnaligned(reinterpret_cast<const uint16_t *>(data.data()), stride,
                         reinterpret_cast<uint16_t *>(addr), (uint32_t) info.stride,
                         (uint32_t) info.width,
                         (uint32_t) info.height);
  } else {
    uint32_t copyWidth = (uint32_t) info.width * 4;
    if (colorConfig == "RGBA_1010102") {
      copyWidth = (uint32_t) info.width;
      coder::CopyUnaligned(reinterpret_cast<const uint32_t *>(data.data()), stride,
                           reinterpret_cast<uint32_t *>(addr), (uint32_t) info.stride,
                           copyWidth,
                           (uint32_t) info.height);
    } else {
      if (use16Floats) {
        coder::CopyUnaligned(reinterpret_cast<const uint16_t *>(data.data()), stride,
                             reinterpret_cast<uint16_t *>(addr), (uint32_t) info.stride,
                             copyWidth,
                             (uint32_t) info.height);
      } else {
        coder::CopyUnaligned(reinterpret_cast<const uint8_t *>(data.data()), stride,
                             reinterpret_cast<uint8_t *>(addr), (uint32_t) info.stride,
                             copyWidth,
                             (uint32_t) info.height);
      }
    }
  }

  if (AndroidBitmap_unlockPixels(env, bitmapObj) != 0) {
    throwPixelsException(env);
    return static_cast<jobject>(nullptr);
  }

  return bitmapObj;
}