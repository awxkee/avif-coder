/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 15/9/2023
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

#include <jni.h>
#include "JniException.h"
#include "Support.h"
#include "SizeScaler.h"

bool checkDecodePreconditions(JNIEnv *env, jint javaColorspace, PreferredColorConfig *config,
                              jint javaScaleMode, ScaleMode *scaleMode, jint javaToneMapper,
                              CurveToneMapper *toneMapper) {
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

    if (preferredColorConfig == Hardware && osVersion < 29) {
        std::string errorString =
                "Color Config HARDWARE supported only 29+ OS version but current is: " +
                std::to_string(osVersion);
        throwException(env, errorString);
        return false;
    }

    auto mScaleMode = static_cast<ScaleMode>(javaScaleMode);
    if (!mScaleMode) {
        std::string errorString = "Invalid Scale Mode was passed";
        throwException(env, errorString);
        return false;
    }

    auto mToneMapper = static_cast<CurveToneMapper>(javaToneMapper);
    if (!mScaleMode) {
        std::string errorString = "Invalid Tone mapper was requestedd";
        throwException(env, errorString);
        return false;
    }

    *scaleMode = mScaleMode;
    *config = preferredColorConfig;
    *toneMapper = mToneMapper;
    return true;
}