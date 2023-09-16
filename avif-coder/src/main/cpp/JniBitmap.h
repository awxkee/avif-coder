//
// Created by Radzivon Bartoshyk on 16/09/2023.
//

#ifndef AVIF_JNIBITMAP_H
#define AVIF_JNIBITMAP_H

#include <jni.h>
#include <vector>

jobject
createBitmap(JNIEnv *env, std::shared_ptr<uint8_t> &data, std::string &colorConfig, int stride,
             int imageWidth, int imageHeight, bool use16Floats, jobject hwBuffer);

#endif //AVIF_JNIBITMAP_H
