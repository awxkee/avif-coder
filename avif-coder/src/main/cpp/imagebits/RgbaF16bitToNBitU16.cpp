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

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "imagebits/RgbaF16bitToNBitU16.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

#include "algo/math-inl.h"
#include "attenuate-inl.h"
#include "RgbaF16bitToNBitU16.h"
#include "half.hpp"
#include <algorithm>
#include "concurrency.hpp"

using namespace std;

HWY_BEFORE_NAMESPACE();
namespace coder::HWY_NAMESPACE {

    using namespace hwy::HWY_NAMESPACE;

    void
    RGBAF16BitToNU16HWY(const uint16_t *sourceData, int srcStride, uint16_t *dst, int dstStride,
                        int width, int height, int bitDepth) {
        auto srcData = reinterpret_cast<const uint8_t *>(sourceData);
        auto data64Ptr = reinterpret_cast<uint8_t *>(dst);
        const float maxColors = std::powf(2.0f, static_cast<float>(bitDepth)) - 1.f;

        const FixedTag<hwy::float16_t, 8> df16;

        const FixedTag<hwy::float32_t, 4> df;
        const FixedTag<uint16_t, 8> dfu16;
        const FixedTag<uint16_t, 4> dfu16x4;
        const FixedTag<uint32_t, 4> du;

        using VF16 = Vec<decltype(df16)>;
        using VF = Vec<decltype(df)>;
        using VU = Vec<decltype(dfu16)>;

        const VF zeros = Zero(df);
        const VF vMaxColors = Set(df, maxColors);

        for (int y = 0; y < height; ++y) {
            auto srcPtr = reinterpret_cast<const uint16_t *>(srcData);
            auto dstPtr = reinterpret_cast<uint16_t *>(data64Ptr);
            int x = 0;
            for (; x + 2 < width; x += 2) {
                VF16 uPixels = LoadU(df16, reinterpret_cast<const hwy::float16_t *>(srcPtr));

                auto low = Clamp(Mul(PromoteLowerTo(df, uPixels), vMaxColors), zeros, vMaxColors);
                auto high = Clamp(Mul(PromoteUpperTo(df, uPixels), vMaxColors), zeros, vMaxColors);
                auto df16Pixels = Combine(dfu16,
                                          DemoteTo(dfu16x4, ConvertTo(du, high)),
                                          DemoteTo(dfu16x4, ConvertTo(du, low)));

                StoreU(df16Pixels, dfu16, dstPtr);
                srcPtr += 8;
                dstPtr += 8;
            }

            for (; x < width; ++x) {
                auto tmpR = (uint16_t) std::clamp(std::roundf(LoadHalf(srcPtr[0]) * maxColors),
                                                  0.0f, maxColors);
                auto tmpG = (uint16_t) std::clamp(std::roundf(LoadHalf(srcPtr[1]) * maxColors),
                                                  0.0f, maxColors);
                auto tmpB = (uint16_t) std::clamp(std::roundf(LoadHalf(srcPtr[2]) * maxColors),
                                                  0.0f, maxColors);
                auto tmpA = (uint16_t) std::clamp(std::roundf(LoadHalf(srcPtr[3]) * maxColors),
                                                  0.0f, maxColors);

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

}
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(RGBAF16BitToNU16HWY);

    void
    RGBAF16BitToNBitU16(const uint16_t *sourceData, int srcStride, uint16_t *dst, int dstStride,
                        int width,
                        int height, int bitDepth) {
        HWY_DYNAMIC_DISPATCH(RGBAF16BitToNU16HWY)(sourceData, srcStride, dst, dstStride, width,
                                                  height, bitDepth);
    }

    void RGBAF16BitToNU16C(const uint16_t *sourceData, int srcStride,
                           uint16_t *dst, int dstStride, int width,
                           int height, int bitDepth) {
        auto srcData = reinterpret_cast<const uint8_t *>(sourceData);
        auto data64Ptr = reinterpret_cast<uint8_t *>(dst);
        const float scale = 1.0f / float((1 << bitDepth) - 1);
        const float maxColors = (float) std::powf(2.0f, static_cast<float>(bitDepth)) - 1.f;

        concurrency::parallel_for(2, height, [&](int y) {
            auto srcPtr = reinterpret_cast<const uint16_t *>(srcData);
            auto dstPtr = reinterpret_cast<uint16_t *>(data64Ptr);
            for (int x = 0; x < width; ++x) {
                auto alpha = LoadHalf(srcPtr[3]);
                auto tmpR = static_cast<uint16_t>(std::clamp(LoadHalf(srcPtr[0]) * maxColors, 0.0f,
                                                             maxColors));
                auto tmpG = static_cast<uint16_t>(std::clamp(LoadHalf(srcPtr[1]) * maxColors, 0.0f,
                                                             maxColors));
                auto tmpB = static_cast<uint16_t>(std::clamp(LoadHalf(srcPtr[2]) * maxColors, 0.0f,
                                                             maxColors));
                auto tmpA = static_cast<uint16_t>(std::clamp((alpha / scale), 0.0f, maxColors));

                dstPtr[0] = tmpR;
                dstPtr[1] = tmpG;
                dstPtr[2] = tmpB;
                dstPtr[3] = tmpA;

                srcPtr += 4;
                dstPtr += 4;
            }

            srcData += srcStride;
            data64Ptr += dstStride;
        });
    }

}
#endif