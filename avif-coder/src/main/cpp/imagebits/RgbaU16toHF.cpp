/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 22/11/2023
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

#include "RgbaU16toHF.h"

using namespace std;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "imagebits/RgbaU16toHF.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "hwy/base.h"
#include "half.hpp"
#include <thread>

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

    using hwy::HWY_NAMESPACE::ScalableTag;
    using hwy::HWY_NAMESPACE::Vec;
    using hwy::HWY_NAMESPACE::Set;
    using hwy::HWY_NAMESPACE::LoadU;
    using hwy::HWY_NAMESPACE::PromoteUpperTo;
    using hwy::HWY_NAMESPACE::PromoteLowerTo;
    using hwy::HWY_NAMESPACE::Mul;
    using hwy::HWY_NAMESPACE::Combine;
    using hwy::HWY_NAMESPACE::FixedTag;
    using hwy::HWY_NAMESPACE::DemoteTo;
    using hwy::HWY_NAMESPACE::ConvertTo;
    using hwy::HWY_NAMESPACE::StoreU;
    using hwy::float16_t;
    using hwy::float32_t;

    void RgbaU16ToFHWYRow(const uint16_t *src,
                          uint16_t *dst,
                          const int width, const float scale) {
        int x = 0;
        const ScalableTag<float16_t> df16;
        const ScalableTag<uint16_t> du16;
        const FixedTag<uint32_t, df16.MaxLanes() / 2> du32;
        const FixedTag<float32_t, df16.MaxLanes() / 2> df32;
        const FixedTag<float16_t, df16.MaxLanes() / 2> df16h;
        int lanes = df16.MaxLanes();
        using VF16 = Vec<decltype(df16)>;
        using VU16 = Vec<decltype(du16)>;
        using VF32 = Vec<decltype(df32)>;
        const VF32 vScale = Set(df32, scale);
        for (; x + lanes < width; x += lanes) {
            VU16 ulane;
            ulane = LoadU(du16, reinterpret_cast<const uint16_t *>(src));
            VF16 lane = Combine(df16,
                                DemoteTo(df16h,
                                         Mul(ConvertTo(df32, PromoteUpperTo(du32, ulane)), vScale)),
                                DemoteTo(df16h, Mul(ConvertTo(df32, PromoteLowerTo(du32, ulane)),
                                                    vScale)));
            StoreU(lane, df16, reinterpret_cast<float16_t *>(dst));
            src += lanes;
            dst += lanes;
        }

        for (; x < width; ++x) {
            uint16_t px = src[0];
            float fpx = static_cast<float>(px) * scale;
            uint16_t hpx = half_float::half(fpx).data_;
            dst[0] = hpx;
            src += 1;
            dst += 1;
        }
    }

    void RgbaU16ToF_HWY(const uint16_t *src, const int srcStride,
                        uint16_t *dst, const int dstStride, const int width,
                        const int height, const int bitDepth) {
        const float scale = 1.0f / (pow(2.0f, static_cast<float>(bitDepth)) - 1.0f);
        for (int y = 0; y < height; ++y) {
            RgbaU16ToFHWYRow(
                    reinterpret_cast<const uint16_t *>(reinterpret_cast<const uint8_t *>(src) +
                                                       y * srcStride),
                    reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t *>(dst) + y * dstStride),
                    width, scale);
        }
    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(RgbaU16ToF_HWY);
    HWY_DLLEXPORT void RgbaU16ToF(const uint16_t *src, const int srcStride,
                                  uint16_t *dst, const int dstStride, const int width,
                                  const int height, const int bitDepth) {
        HWY_DYNAMIC_DISPATCH(RgbaU16ToF_HWY)(src, srcStride, dst, dstStride, width,
                                             height, bitDepth);
    }
}
#endif