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

#include "Rgb565.h"
#include <thread>
#include <vector>
#include "half.hpp"
#include <algorithm>

using namespace std;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "imagebits/Rgb565.cpp"

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
    using hwy::HWY_NAMESPACE::Round;
    using hwy::HWY_NAMESPACE::ConvertTo;
    using hwy::HWY_NAMESPACE::PromoteTo;
    using hwy::HWY_NAMESPACE::DemoteTo;
    using hwy::HWY_NAMESPACE::PromoteLowerTo;
    using hwy::HWY_NAMESPACE::PromoteUpperTo;
    using hwy::HWY_NAMESPACE::Combine;
    using hwy::HWY_NAMESPACE::Rebind;
    using hwy::HWY_NAMESPACE::LowerHalf;
    using hwy::HWY_NAMESPACE::ShiftLeft;
    using hwy::HWY_NAMESPACE::ShiftRight;
    using hwy::HWY_NAMESPACE::UpperHalf;
    using hwy::HWY_NAMESPACE::LoadInterleaved4;
    using hwy::HWY_NAMESPACE::StoreU;
    using hwy::HWY_NAMESPACE::Or;
    using hwy::float16_t;

    void
    Rgba8To565HWYRow(const uint8_t *source, uint16_t *destination, int width) {
        const FixedTag<uint16_t, 8> du16;
        const FixedTag<uint8_t, 16> du8x16;
        using VU16 = Vec<decltype(du16)>;
        using VU8x16 = Vec<decltype(du8x16)>;

        int x = 0;
        int pixels = 16;

        auto src = reinterpret_cast<const uint8_t *>(source);
        auto dst = reinterpret_cast<uint16_t *>(destination);
        for (x = 0; x + pixels < width; x += pixels) {
            VU8x16 ru8Row;
            VU8x16 gu8Row;
            VU8x16 bu8Row;
            VU8x16 au8Row;

            LoadInterleaved4(du8x16, reinterpret_cast<const uint8_t *>(src),
                             ru8Row, gu8Row, bu8Row, au8Row);

            auto rdu16Vec = ShiftLeft<11>(ShiftRight<3>(PromoteLowerTo(du16, ru8Row)));
            auto gdu16Vec = ShiftLeft<5>(ShiftRight<2>(PromoteLowerTo(du16, gu8Row)));
            auto bdu16Vec = ShiftRight<3>(PromoteLowerTo(du16, bu8Row));

            auto result = Or(Or(rdu16Vec, gdu16Vec), bdu16Vec);
            StoreU(result, du16, dst);

            rdu16Vec = ShiftLeft<11>(ShiftRight<3>(PromoteUpperTo(du16, ru8Row)));
            gdu16Vec = ShiftLeft<5>(ShiftRight<2>(PromoteUpperTo(du16, gu8Row)));
            bdu16Vec = ShiftRight<3>(PromoteUpperTo(du16, bu8Row));

            result = Or(Or(rdu16Vec, gdu16Vec), bdu16Vec);
            Store(result, du16, dst + 8);

            src += 4 * pixels;
            dst += pixels;
        }

        for (; x < width; ++x) {
            uint16_t red565 = (src[0] >> 3) << 11;
            uint16_t green565 = (src[1] >> 2) << 5;
            uint16_t blue565 = src[2] >> 3;

            auto result = static_cast<uint16_t>(red565 | green565 | blue565);
            dst[0] = result;

            src += 4;
            dst += 1;
        }

    }

    void Rgba8To565HWY(const uint8_t *sourceData, int srcStride,
                       uint16_t *dst, int dstStride, int width,
                       int height, int bitDepth) {
        auto mSrc = reinterpret_cast<const uint8_t *>(sourceData);
        auto mDst = reinterpret_cast<uint8_t *>(dst);

        int threadCount = clamp(min(static_cast<int>(std::thread::hardware_concurrency()),
                                    height * width / (256 * 256)), 1, 12);
        std::vector<std::thread> workers;

        int segmentHeight = height / threadCount;

        for (int i = 0; i < threadCount; i++) {
            int start = i * segmentHeight;
            int end = (i + 1) * segmentHeight;
            if (i == threadCount - 1) {
                end = height;
            }
            workers.emplace_back(
                    [start, end, mSrc, dstStride, mDst, srcStride, width]() {
                        for (int y = start; y < end; ++y) {
                            Rgba8To565HWYRow(
                                    reinterpret_cast<const uint8_t *>(mSrc + y * srcStride),
                                    reinterpret_cast<uint16_t *>(mDst + y * dstStride), width);
                        }
                    });
        }

        for (std::thread &thread: workers) {
            thread.join();
        }
    }

    inline Vec<FixedTag<uint8_t, 8>>
    ConvertF16ToU16Row(Vec<FixedTag<uint16_t, 8>> v, const float maxColors) {
        FixedTag<float16_t, 4> df16;
        FixedTag<uint16_t, 4> dfu416;
        FixedTag<uint8_t, 8> du8;
        Rebind<float, decltype(df16)> rf32;
        Rebind<int32_t, decltype(rf32)> ri32;
        Rebind<uint8_t, decltype(rf32)> ru8;

        using VU8 = Vec<decltype(du8)>;

        const auto minColors = Zero(rf32);
        const auto vMaxColors = Set(rf32, static_cast<float>(maxColors));

        auto lower = DemoteTo(ru8, ConvertTo(ri32,
                                             Max(Min(Round(Mul(
                                                     PromoteTo(rf32, BitCast(df16, LowerHalf(v))),
                                                     vMaxColors)), vMaxColors), minColors)
        ));
        auto upper = DemoteTo(ru8, ConvertTo(ri32,
                                             Max(Min(Round(Mul(PromoteTo(rf32,
                                                                         BitCast(df16,
                                                                                 UpperHalf(dfu416,
                                                                                           v))),
                                                               vMaxColors)), vMaxColors), minColors)
        ));
        return Combine(du8, upper, lower);
    }

    void
    RGBAF16To565RowHWY(const uint16_t *source, uint16_t *destination, int width,
                       const float maxColors) {
        const FixedTag<uint16_t, 8> du16;
        const FixedTag<uint8_t, 8> du8;
        using VU16 = Vec<decltype(du16)>;
        using VU8 = Vec<decltype(du8)>;

        Rebind<uint16_t, FixedTag<uint8_t, 8>> rdu16;

        int x = 0;
        int pixels = 8;

        auto src = reinterpret_cast<const uint16_t *>(source);
        auto dst = reinterpret_cast<uint16_t *>(destination);
        for (x = 0; x + pixels < width; x += pixels) {
            VU16 ru16Row;
            VU16 gu16Row;
            VU16 bu16Row;
            VU16 au16Row;
            LoadInterleaved4(du16, reinterpret_cast<const uint16_t *>(src),
                             ru16Row, gu16Row,
                             bu16Row,
                             au16Row);

            auto r16Row = ConvertF16ToU16Row(ru16Row, maxColors);
            auto g16Row = ConvertF16ToU16Row(gu16Row, maxColors);
            auto b16Row = ConvertF16ToU16Row(bu16Row, maxColors);

            auto rdu16Vec = ShiftLeft<11>(ShiftRight<3>(PromoteTo(rdu16, r16Row)));
            auto gdu16Vec = ShiftLeft<5>(ShiftRight<2>(PromoteTo(rdu16, g16Row)));
            auto bdu16Vec = ShiftRight<3>(PromoteTo(rdu16, b16Row));

            auto result = Or(Or(rdu16Vec, gdu16Vec), bdu16Vec);
            StoreU(result, du16, dst);

            src += 4 * pixels;
            dst += pixels;
        }

        for (; x < width; ++x) {
            uint16_t red565 = ((uint16_t) round(LoadHalf(src[0]) * maxColors) >> 3) << 11;
            uint16_t green565 = ((uint16_t) round(LoadHalf(src[1]) * maxColors) >> 2) << 5;
            uint16_t blue565 = (uint16_t) round(LoadHalf(src[2]) * maxColors) >> 3;

            auto result = static_cast<uint16_t>(red565 | green565 | blue565);
            dst[0] = result;

            src += 4;
            dst += 1;
        }

    }

    void RGBAF16To565HWY(const uint16_t *sourceData, int srcStride,
                         uint16_t *dst, int dstStride, int width,
                         int height) {
        const float maxColors = powf(2, (float) 8) - 1;

        auto mSrc = reinterpret_cast<const uint8_t *>(sourceData);
        auto mDst = reinterpret_cast<uint8_t *>(dst);

        int threadCount = clamp(min(static_cast<int>(std::thread::hardware_concurrency()),
                                    height * width / (256 * 256)), 1, 12);
        std::vector<std::thread> workers;

        int segmentHeight = height / threadCount;

        for (int i = 0; i < threadCount; i++) {
            int start = i * segmentHeight;
            int end = (i + 1) * segmentHeight;
            if (i == threadCount - 1) {
                end = height;
            }
            workers.emplace_back(
                    [start, end, mSrc, dstStride, mDst, maxColors, srcStride, width]() {
                        for (int y = start; y < end; ++y) {
                            RGBAF16To565RowHWY(
                                    reinterpret_cast<const uint16_t *>(mSrc + y * srcStride),
                                    reinterpret_cast<uint16_t *>(mDst + y * dstStride), width,
                                    maxColors);
                        }
                    });
        }

        for (std::thread &thread: workers) {
            thread.join();
        }
    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(Rgba8To565HWY);
    HWY_DLLEXPORT void Rgba8To565(const uint8_t *sourceData, int srcStride,
                                  uint16_t *dst, int dstStride, int width,
                                  int height, int bitDepth) {
        HWY_DYNAMIC_DISPATCH(Rgba8To565HWY)(sourceData, srcStride, dst, dstStride, width,
                                            height, bitDepth);
    }

    HWY_EXPORT(RGBAF16To565HWY);
    HWY_DLLEXPORT void RGBAF16To565(const uint16_t *sourceData, int srcStride,
                                    uint16_t *dst, int dstStride, int width,
                                    int height) {
        HWY_DYNAMIC_DISPATCH(RGBAF16To565HWY)(sourceData, srcStride, dst, dstStride, width,
                                              height);
    }
}
#endif