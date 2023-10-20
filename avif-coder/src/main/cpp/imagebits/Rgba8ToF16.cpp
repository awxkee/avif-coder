/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 15/09/2023
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

#include "Rgba8ToF16.h"
#include "ThreadPool.hpp"
#include "half.hpp"

using namespace half_float;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "imagebits/Rgba8ToF16.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

    using hwy::HWY_NAMESPACE::Set;
    using hwy::HWY_NAMESPACE::FixedTag;
    using hwy::HWY_NAMESPACE::Vec;
    using hwy::HWY_NAMESPACE::Mul;
    using hwy::HWY_NAMESPACE::Max;
    using hwy::HWY_NAMESPACE::Min;
    using hwy::HWY_NAMESPACE::Zero;
    using hwy::HWY_NAMESPACE::BitCast;
    using hwy::HWY_NAMESPACE::ConvertTo;
    using hwy::HWY_NAMESPACE::PromoteTo;
    using hwy::HWY_NAMESPACE::DemoteTo;
    using hwy::HWY_NAMESPACE::Combine;
    using hwy::HWY_NAMESPACE::Rebind;
    using hwy::HWY_NAMESPACE::LowerHalf;
    using hwy::HWY_NAMESPACE::UpperHalf;
    using hwy::HWY_NAMESPACE::LoadInterleaved4;
    using hwy::HWY_NAMESPACE::StoreInterleaved4;
    using hwy::float16_t;
    using hwy::float32_t;

    inline Vec<FixedTag<uint16_t, 8>>
    ConvertRow(Vec<FixedTag<uint8_t, 8>> v, float scale) {
        FixedTag<uint8_t, 4> du8x4;
        FixedTag<uint16_t, 8> du16x8;
        Rebind<int32_t, FixedTag<uint8_t, 4>> ri32;
        Rebind<float32_t, decltype(ri32)> df32;
        Rebind<float16_t, decltype(df32)> dff16;
        Rebind<uint16_t, decltype(dff16)> du16;
        auto lower = BitCast(du16, DemoteTo(dff16,
                                            Mul(ConvertTo(df32, PromoteTo(ri32, LowerHalf(v))),
                                                Set(df32, scale))));
        auto upper = BitCast(du16, DemoteTo(dff16,
                                            Mul(ConvertTo(df32,
                                                          PromoteTo(ri32, UpperHalf(du8x4, v))),
                                                Set(df32, scale))));
        return Combine(du16x8, upper, lower);
    }

    void
    Rgba8ToF16HWYRow(const uint8_t *source, uint16_t *destination, int width, float scale) {
        const FixedTag<uint16_t, 8> du16;
        const FixedTag<uint8_t, 8> du8x8;
        using VU16 = Vec<decltype(du16)>;
        using VU8x8 = Vec<decltype(du8x8)>;

        int x = 0;
        int pixels = 8;

        auto src = reinterpret_cast<const uint8_t *>(source);
        auto dst = reinterpret_cast<uint16_t *>(destination);
        for (x = 0; x + pixels < width; x += pixels) {
            VU8x8 ru8Row;
            VU8x8 gu8Row;
            VU8x8 bu8Row;
            VU8x8 au8Row;
            LoadInterleaved4(du8x8, reinterpret_cast<const uint8_t *>(src),
                             ru8Row, gu8Row, bu8Row, au8Row);

            auto r16Row = ConvertRow(ru8Row, scale);
            auto g16Row = ConvertRow(gu8Row, scale);
            auto b16Row = ConvertRow(bu8Row, scale);
            auto a16Row = ConvertRow(au8Row, scale);

            StoreInterleaved4(r16Row, g16Row, b16Row, a16Row, du16, dst);

            src += 4 * pixels;
            dst += 4 * pixels;
        }

        for (; x < width; ++x) {
            auto tmpR = (uint16_t) half((float) src[0] * scale).data_;
            auto tmpG = (uint16_t) half((float) src[1] * scale).data_;
            auto tmpB = (uint16_t) half((float) src[2] * scale).data_;
            auto tmpA = (uint16_t) half((float) src[3] * scale).data_;

            dst[0] = tmpR;
            dst[1] = tmpG;
            dst[2] = tmpB;
            dst[3] = tmpA;

            src += 4;
            dst += 4;
        }

    }

    void Rgba8ToF16HWY(const uint8_t *sourceData, int srcStride,
                       uint16_t *dst, int dstStride, int width,
                       int height, int bitDepth) {
        auto mSrc = reinterpret_cast<const uint8_t *>(sourceData);
        auto mDst = reinterpret_cast<uint8_t *>(dst);

        const float scale = 1.0f / float((1 << bitDepth) - 1);

        int minimumTilingAreaSize = 850 * 850;

        int currentAreaSize = width * height;
        if (minimumTilingAreaSize > currentAreaSize) {
            ThreadPool pool;
            std::vector<std::future<void>> results;

            for (int y = 0; y < height; ++y) {
                auto r = pool.enqueue(Rgba8ToF16HWYRow,
                                      reinterpret_cast<const uint8_t *>(mSrc),
                                      reinterpret_cast<uint16_t *>(mDst), width, scale);
                results.push_back(std::move(r));
                mSrc += srcStride;
                mDst += dstStride;
            }

            for (auto &result: results) {
                result.wait();
            }
        } else {
            for (int y = 0; y < height; ++y) {
                Rgba8ToF16HWYRow(reinterpret_cast<const uint8_t *>(mSrc),
                                 reinterpret_cast<uint16_t *>(mDst), width, scale);
                mSrc += srcStride;
                mDst += dstStride;
            }
        }

    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(Rgba8ToF16HWY);
    HWY_DLLEXPORT void Rgba8ToF16(const uint8_t *sourceData, int srcStride,
                                  uint16_t *dst, int dstStride, int width,
                                  int height, int bitDepth) {
        HWY_DYNAMIC_DISPATCH(Rgba8ToF16HWY)(sourceData, srcStride, dst, dstStride, width,
                                            height, bitDepth);
    }
}
#endif