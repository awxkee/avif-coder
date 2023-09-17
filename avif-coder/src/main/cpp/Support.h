//
// Created by Radzivon Bartoshyk on 15/09/2023.
//

#ifndef AVIF_SUPPORT_H
#define AVIF_SUPPORT_H

#include <jni.h>
#include "SizeScaler.h"

enum PreferredColorConfig {
    Default = 1,
    Rgba_8888 = 2,
    Rgba_F16 = 3,
    Rgb_565 = 4,
    Rgba_1010102 = 5,
    Hardware = 6
};

bool checkDecodePreconditions(JNIEnv *env, jint javaColorspace, PreferredColorConfig *config,
                              jint javaScaleMode, ScaleMode *scaleMode);

#endif //AVIF_SUPPORT_H
