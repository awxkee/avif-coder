//
// Created by Radzivon Bartoshyk on 01/01/2023.
//

#ifndef AVIF_JNI_EXCEPTION_H
#define AVIF_JNI_EXCEPTION_H

#include <jni.h>

jint throwCannotReadFileException(JNIEnv *env);
jint throwBitDepthException(JNIEnv *env);
jint throwCoderCreationException(JNIEnv *env);
jint throwCantDecodeImageException(JNIEnv *env);
jint throwInvalidScale(JNIEnv *env);
jint throwCantEncodeImageException(JNIEnv *env);
jint throwInvalidPixelsFormat(JNIEnv *env);
jint throwPixelsException(JNIEnv *env);

#endif //AVIF_JNI_EXCEPTION_H
