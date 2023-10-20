/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 05/09/2023
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

#include "RgbaF16bitToNBitU16.h"
#include "half.hpp"
#include <algorithm>

using namespace std;

#if HAVE_NEON

#include <arm_neon.h>

void RGBAF16BitToNU16NEON(const uint16_t *sourceData, int srcStride, uint16_t *dst, int dstStride,
                          int width, int height, int bitDepth) {
    auto srcData = reinterpret_cast<const uint8_t *>(sourceData);
    auto data64Ptr = reinterpret_cast<uint8_t *>(dst);
    const float scale = 1.0f / float((1 << bitDepth) - 1);
    auto vectorScale = vdupq_n_f32(scale);
    float maxColors = (float) pow(2.0, (double) bitDepth) - 1;
    auto maxValue = vdupq_n_f32((float) maxColors);
    auto minValue = vdupq_n_f32(0);

    for (int y = 0; y < height; ++y) {
        auto srcPtr = reinterpret_cast<const uint16_t *>(srcData);
        auto dstPtr = reinterpret_cast<uint16_t *>(data64Ptr);
        int x;
        for (x = 0; x + 2 < width; x += 2) {
            float16x8_t neonSrc = vld1q_f16(reinterpret_cast<const __fp16 *>(srcPtr));

            float32x4_t neonFloat32_1 = vminq_f32(
                    vmaxq_f32(vdivq_f32(vcvt_f32_f16(vget_low_f16(neonSrc)), vectorScale),
                              minValue), maxValue);
            float32x4_t neonFloat32_2 = vminq_f32(
                    vmaxq_f32(vdivq_f32(vcvt_f32_f16(vget_high_f16(neonSrc)), vectorScale),
                              minValue), maxValue);

            uint32x4_t neonSigned1 = vcvtq_u32_f32(neonFloat32_1);
            uint32x4_t neonSigned2 = vcvtq_u32_f32(neonFloat32_2);

            uint16x4_t neonUInt16_1 = vqmovn_u32(neonSigned1);
            uint16x4_t neonUInt16_2 = vqmovn_u32(neonSigned2);

            vst1_u16(dstPtr, neonUInt16_1);
            vst1_u16(dstPtr + 4, neonUInt16_2);

            srcPtr += 8;
            dstPtr += 8;
        }

        for (; x < width; ++x) {
            auto alpha = LoadHalf(srcPtr[3]);
            auto tmpR = (uint16_t) std::clamp(LoadHalf(srcPtr[0]) / scale, 0.0f, maxColors);
            auto tmpG = (uint16_t) std::clamp(LoadHalf(srcPtr[1]) / scale, 0.0f, maxColors);
            auto tmpB = (uint16_t) std::clamp(LoadHalf(srcPtr[2]) / scale, 0.0f, maxColors);
            auto tmpA = (uint16_t) std::clamp((alpha / scale), 0.0f, maxColors);

            dstPtr[0] = tmpR;
            dstPtr[1] = tmpG;
            dstPtr[2] = tmpB;
            dstPtr[3] = tmpA;

            srcPtr += 4;
            dstPtr += 4;
        }

        srcData += srcStride;
        data64Ptr += dstStride;
    }
}

#endif

void RGBAF16BitToNU16C(const uint16_t *sourceData, int srcStride,
                       uint16_t *dst, int dstStride, int width,
                       int height, int bitDepth) {
    auto srcData = reinterpret_cast<const uint8_t *>(sourceData);
    auto data64Ptr = reinterpret_cast<uint8_t *>(dst);
    const float scale = 1.0f / float((1 << bitDepth) - 1);
    float maxColors = (float) pow(2.0, (double) bitDepth) - 1;

    for (int y = 0; y < height; ++y) {
        auto srcPtr = reinterpret_cast<const uint16_t *>(srcData);
        auto dstPtr = reinterpret_cast<uint16_t *>(data64Ptr);
        for (int x = 0; x < width; ++x) {
            auto alpha = LoadHalf(srcPtr[3]);
            auto tmpR = (uint16_t) clamp(LoadHalf(srcPtr[0]) / scale, 0.0f, maxColors);
            auto tmpG = (uint16_t) clamp(LoadHalf(srcPtr[1]) / scale, 0.0f, maxColors);
            auto tmpB = (uint16_t) clamp(LoadHalf(srcPtr[2]) / scale, 0.0f, maxColors);
            auto tmpA = (uint16_t) clamp((alpha / scale), 0.0f, maxColors);

            dstPtr[0] = tmpR;
            dstPtr[1] = tmpG;
            dstPtr[2] = tmpB;
            dstPtr[3] = tmpA;

            srcPtr += 4;
            dstPtr += 4;
        }

        srcData += srcStride;
        data64Ptr += dstStride;
    }
}

void
RGBAF16BitToNBitU16(const uint16_t *sourceData, int srcStride, uint16_t *dst, int dstStride,
                    int width,
                    int height, int bitDepth) {
#if HAVE_NEON
    RGBAF16BitToNU16NEON(sourceData, srcStride, dst, dstStride, width, height, bitDepth);
#else
    RGBAF16BitToNU16C(sourceData, srcStride, dst, dstStride, width, height, bitDepth);
#endif
}