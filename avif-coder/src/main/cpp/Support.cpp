//
// Created by Radzivon Bartoshyk on 16/09/2023.
//

#include <jni.h>
#include "JniException.h"
#include "Support.h"

bool checkDecodePreconditions(JNIEnv *env, jint javaColorspace, PreferredColorConfig *config) {
    auto preferredColorConfig = static_cast<PreferredColorConfig>(javaColorspace);
    if (!preferredColorConfig) {
        std::string errorString =
                "Invalid Color Config: " + std::to_string(javaColorspace) + " was passed";
        throwException(env, errorString);
        return false;
    }

    int osVersion = androidOSVersion();

    if (preferredColorConfig == Rgba_1010102 && osVersion < 33) {
        std::string errorString =
                "Color Config RGBA_1010102 supported only 33+ OS version but current is: " +
                std::to_string(osVersion);
        throwException(env, errorString);
        return false;
    }

    if (preferredColorConfig == Rgba_F16 && osVersion < 26) {
        std::string errorString =
                "Color Config RGBA_1010102 supported only 26+ OS version but current is: " +
                std::to_string(osVersion);
        throwException(env, errorString);
        return false;
    }

    *config = preferredColorConfig;
    return true;
}