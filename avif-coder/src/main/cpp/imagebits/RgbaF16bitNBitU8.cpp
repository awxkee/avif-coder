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

#include "RgbaF16bitNBitU8.h"
#include "half.hpp"
#include <algorithm>
#include "ThreadPool.hpp"

using namespace std;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "imagebits/RgbaF16bitNBitU8.cpp"

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

    inline Vec<FixedTag<uint8_t, 8>>
    ConvertRow(Vec<FixedTag<uint16_t, 8>> v, float maxColors) {
        FixedTag<float16_t, 4> df16;
        FixedTag<uint16_t, 4> dfu416;
        FixedTag<uint8_t, 8> du8;
        Rebind<float, decltype(df16)> rf32;
        Rebind<int32_t, decltype(rf32)> ri32;
        Rebind<uint8_t, decltype(rf32)> ru8;

        using VU8 = Vec<decltype(du8)>;

        auto minColors = Zero(rf32);
        auto vMaxColors = Set(rf32, (int) maxColors);

        auto lower = DemoteTo(ru8, ConvertTo(ri32,
                                             Max(Min(Mul(
                                                     PromoteTo(rf32, BitCast(df16, LowerHalf(v))),
                                                     vMaxColors), vMaxColors), minColors)
        ));
        auto upper = DemoteTo(ru8, ConvertTo(ri32,
                                             Max(Min(Mul(PromoteTo(rf32,
                                                                   BitCast(df16,
                                                                           UpperHalf(dfu416, v))),
                                                         vMaxColors), vMaxColors), minColors)
        ));
        return Combine(du8, upper, lower);
    }

    void
    RGBAF16BitToNBitRowU8(const uint16_t *source, uint8_t *destination, int width, float scale,
                          float maxColors) {
        const FixedTag<uint16_t, 8> du16;
        const FixedTag<uint8_t, 8> du8;
        using VU16 = Vec<decltype(du16)>;
        using VU8 = Vec<decltype(du8)>;

        int x = 0;
        int pixels = 8;

        auto src = reinterpret_cast<const uint16_t *>(source);
        auto dst = reinterpret_cast<uint8_t *>(destination);
        for (x = 0; x + pixels < width; x += pixels) {
            VU16 ru16Row;
            VU16 gu16Row;
            VU16 bu16Row;
            VU16 au16Row;
            LoadInterleaved4(du16, reinterpret_cast<const uint16_t *>(src),
                             ru16Row, gu16Row,
                             bu16Row,
                             au16Row);

            auto r16Row = ConvertRow(ru16Row, maxColors);
            auto g16Row = ConvertRow(gu16Row, maxColors);
            auto b16Row = ConvertRow(bu16Row, maxColors);
            auto a16Row = ConvertRow(au16Row, maxColors);

            StoreInterleaved4(r16Row, g16Row, b16Row, a16Row, du8, dst);

            src += 4 * pixels;
            dst += 4 * pixels;
        }

        for (; x < width; ++x) {
            auto tmpR = (uint16_t) clamp(LoadHalf(src[0]) / scale, 0.0f, maxColors);
            auto tmpG = (uint16_t) clamp(LoadHalf(src[1]) / scale, 0.0f, maxColors);
            auto tmpB = (uint16_t) clamp(LoadHalf(src[2]) / scale, 0.0f, maxColors);
            auto tmpA = (uint16_t) clamp(LoadHalf(src[3]) / scale, 0.0f, maxColors);

            dst[0] = tmpR;
            dst[1] = tmpG;
            dst[2] = tmpB;
            dst[3] = tmpA;

            src += 4;
            dst += 4;
        }

    }

    void RGBAF16BitToNBitU8(const uint16_t *sourceData, int srcStride,
                            uint8_t *dst, int dstStride, int width,
                            int height, int bitDepth) {

        float maxColors = powf(2, (float) bitDepth) - 1;

        auto mSrc = reinterpret_cast<const uint8_t *>(sourceData);
        auto mDst = reinterpret_cast<uint8_t *>(dst);

        const float scale = 1.0f / float((1 << bitDepth) - 1);

        int minimumTilingAreaSize = 850 * 850;

        if (width * height >= minimumTilingAreaSize) {
            ThreadPool pool;
            std::vector<std::future<void>> results;
            for (int y = 0; y < height; ++y) {
                auto r = pool.enqueue(RGBAF16BitToNBitRowU8,
                                      reinterpret_cast<const uint16_t *>(mSrc),
                                      reinterpret_cast<uint8_t *>(mDst), width, scale,
                                      maxColors);
                results.push_back(std::move(r));
                mSrc += srcStride;
                mDst += dstStride;
            }
            for (auto &result: results) {
                result.wait();
            }
        } else {
            for (int y = 0; y < height; ++y) {
                RGBAF16BitToNBitRowU8(reinterpret_cast<const uint16_t *>(mSrc),
                                      reinterpret_cast<uint8_t *>(mDst), width, scale,
                                      maxColors);
                mSrc += srcStride;
                mDst += dstStride;
            }
        }
    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(RGBAF16BitToNBitU8);
    HWY_DLLEXPORT void RGBAF16BitToNBitU8(const uint16_t *sourceData, int srcStride,
                                          uint8_t *dst, int dstStride, int width,
                                          int height, int bitDepth) {
        HWY_DYNAMIC_DISPATCH(RGBAF16BitToNBitU8)(sourceData, srcStride, dst, dstStride, width,
                                                 height, bitDepth);
    }
}
#endif