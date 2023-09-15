//
// Created by Radzivon Bartoshyk on 01/01/2023.
//

#include "JniException.h"

jint throwCannotReadFileException(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/CantReadHeifFileException");
    return env->ThrowNew(exClass, "");
}

jint throwBitDepthException(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/CorruptedBitDepthException");
    return env->ThrowNew(exClass, "");
}

jint throwCoderCreationException(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/CantCreateCodecException");
    return env->ThrowNew(exClass, "");
}

jint throwCantDecodeImageException(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/CantDecoderImageException");
    return env->ThrowNew(exClass, "");
}

jint throwHardwareBitmapException(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/HardwareBitmapIsNotImplementedException");
    return env->ThrowNew(exClass, "");
}

jint throwInvalidScale(JNIEnv *env, const char * msg) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/HeifCantScaleException");
    return env->ThrowNew(exClass, msg);
}

jint throwCantEncodeImageException(JNIEnv *env, const char * msg) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/CantEncodeImageException");
    return env->ThrowNew(exClass, msg);
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