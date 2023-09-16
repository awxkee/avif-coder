//
// Created by Radzivon Bartoshyk on 15/09/2023.
//

#ifndef AVIF_SUPPORT_H
#define AVIF_SUPPORT_H

#include <jni.h>

enum PreferredColorConfig {
    Default = 0,
    Rgba_8888 = 1,
    Rgba_F16 = 2,
    Rgb_565 = 3,
    Rgba_1010102 = 4,
    Hardware = 5
};

bool checkDecodePreconditions(JNIEnv *env, jint javaColorspace, PreferredColorConfig *config);

#endif //AVIF_SUPPORT_H
