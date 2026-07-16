/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 16/9/2023
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

#ifndef AVIF_REFORMATBITMAP_H
#define AVIF_REFORMATBITMAP_H

#include <jni.h>
#include <vector>
#include <string>
#include "Support.h"
#include "definitions.h"

namespace coder {
// Controls verbose AHardwareBuffer lifecycle diagnostics at runtime.
void SetHardwareBufferDebugLoggingEnabled(bool enabled) noexcept;
bool IsHardwareBufferDebugLoggingEnabled() noexcept;

void
ReformatColorConfig(JNIEnv *env, aligned_uint8_vector &imageData, std::string &imageConfig,
                    PreferredColorConfig preferredColorConfig, uint32_t depth,
                    uint32_t imageWidth, uint32_t imageHeight, uint32_t *stride, bool *useFloats,
                    jobject *hwBuffer, bool alphaPremultiplied, bool doesImageHasAlpha);
}

#endif //AVIF_REFORMATBITMAP_H
