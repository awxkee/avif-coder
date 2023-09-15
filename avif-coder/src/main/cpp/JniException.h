//
// Created by Radzivon Bartoshyk on 01/01/2023.
//

#ifndef AVIF_JNIEXCEPTION_H
#define AVIF_JNIEXCEPTION_H

#include <jni.h>
#include <string>

jint throwCannotReadFileException(JNIEnv *env);

jint throwBitDepthException(JNIEnv *env);

jint throwCoderCreationException(JNIEnv *env);

jint throwCantDecodeImageException(JNIEnv *env);

jint throwInvalidScale(JNIEnv *env, const char *msg);

jint throwCantEncodeImageException(JNIEnv *env, const char *msg);

jint throwInvalidPixelsFormat(JNIEnv *env);

jint throwPixelsException(JNIEnv *env);

jint throwHardwareBitmapException(JNIEnv *env);

jint throwException(JNIEnv *env, std::string &msg);

int androidOSVersion();

#endif //AVIF_JNIEXCEPTION_H
