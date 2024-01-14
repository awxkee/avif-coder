/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 23/9/2023
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

#ifndef AVIF_VULKANRUNNER_H
#define AVIF_VULKANRUNNER_H

#include <cstdint>
#include <string>
#include <android/asset_manager_jni.h>

enum PixelFormat {
    RgbaU8 = 1,
    RgbaU32 = 2,
    RgbaF16 = 3,
    RgbaF32 = 4,
    Rgba1010102 = 5,
};

struct ShaderEotfData {
    float colorMatrix[3][4] = {};
    float lumaPrimaries[3] = {};
    int toneMapper; // Rec2408 - 1, Logarithmic - 2
    int oetfCurve; // curve 1 - Rec2020, 2 - P3, 3 - SRGB
} ;

bool loadVulkanRunner();

typedef int (*VulkanComputeRunnerFunc)(
        std::string &kernelName, AAssetManager *assetManager, uint8_t *srcBuffer, int width,
        int height,
        int stride, PixelFormat pixelFormat
);

typedef int (*VulkanComputeRunnerWithDataFunc)(
        std::string &kernelName, AAssetManager *assetManager, uint8_t *srcBuffer, int width,
        int height,
        int stride, PixelFormat pixelFormat, void *data, uint32_t dataSize
);

extern VulkanComputeRunnerWithDataFunc VulkanRunnerWithData;
extern VulkanComputeRunnerFunc VulkanRunner;

#endif //AVIF_VULKANRUNNER_H
