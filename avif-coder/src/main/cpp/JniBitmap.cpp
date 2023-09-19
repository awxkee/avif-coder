//
// Created by Radzivon Bartoshyk on 16/09/2023.
//

#include "JniBitmap.h"
#include <jni.h>
#include <vector>
#include "JniException.h"
#include <android/bitmap.h>
#include "imagebits/CopyUnalignedRGBA.h"

jobject
createBitmap(JNIEnv *env, std::shared_ptr<uint8_t> &data, std::string &colorConfig, int stride,
             int imageWidth, int imageHeight, bool use16Floats, jobject hwBuffer) {
    if (colorConfig == "HARDWARE") {
        jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
        jmethodID createBitmapMethodID = env->GetStaticMethodID(bitmapClass, "wrapHardwareBuffer",
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
    jmethodID createBitmapMethodID = env->GetStaticMethodID(bitmapClass, "createBitmap",
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
        coder::CopyUnalignedRGB565(reinterpret_cast<const uint8_t *>(data.get()), stride,
                                   reinterpret_cast<uint8_t *>(addr), (int) info.stride,
                                   (int) info.width,
                                   (int) info.height);
    } else {
        coder::CopyUnalignedRGBA(reinterpret_cast<const uint8_t *>(data.get()), stride,
                                 reinterpret_cast<uint8_t *>(addr), (int) info.stride,
                                 (int) info.width,
                                 (int) info.height, use16Floats ? 2 : 1);
    }

    if (AndroidBitmap_unlockPixels(env, bitmapObj) != 0) {
        throwPixelsException(env);
        return static_cast<jobject>(nullptr);
    }

    return bitmapObj;
}