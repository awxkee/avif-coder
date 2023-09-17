//
// Created by Radzivon Bartoshyk on 01/01/2023.
//

#include "JniException.h"

jint throwBitDepthException(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/CorruptedBitDepthException");
    return env->ThrowNew(exClass, "");
}

jint throwHardwareBitmapException(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/HardwareBitmapIsNotImplementedException");
    return env->ThrowNew(exClass, "");
}

jint throwInvalidPixelsFormat(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/UnsupportedImageFormatException");
    return env->ThrowNew(exClass, "");
}

jint throwPixelsException(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/GetPixelsException");
    return env->ThrowNew(exClass, "");
}

jint throwException(JNIEnv *env, std::string &msg) {
    jclass exClass;
    exClass = env->FindClass("java/lang/Exception");
    return env->ThrowNew(exClass, msg.c_str());
}

int androidOSVersion() {
    return android_get_device_api_level();
}