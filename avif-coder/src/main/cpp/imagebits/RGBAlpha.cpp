/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 06/11/2023
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

#include "RGBAlpha.h"

using namespace std;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "imagebits/RGBAlpha.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

    using hwy::HWY_NAMESPACE::Vec;
    using hwy::HWY_NAMESPACE::FixedTag;
    using hwy::HWY_NAMESPACE::Min;
    using hwy::HWY_NAMESPACE::LoadInterleaved4;
    using hwy::HWY_NAMESPACE::StoreInterleaved4;
    using hwy::HWY_NAMESPACE::PromoteLowerTo;
    using hwy::HWY_NAMESPACE::PromoteUpperTo;
    using hwy::HWY_NAMESPACE::DemoteTo;
    using hwy::HWY_NAMESPACE::Combine;
    using hwy::HWY_NAMESPACE::Min;
    using hwy::HWY_NAMESPACE::Add;
    using hwy::HWY_NAMESPACE::ShiftRight;
    using hwy::HWY_NAMESPACE::Rebind;
    using hwy::HWY_NAMESPACE::Div;
    using hwy::HWY_NAMESPACE::ConvertTo;
    using hwy::HWY_NAMESPACE::ApproximateReciprocal;

    template<typename D, typename I = Vec<D>>
    inline __attribute__((flatten)) I
    RearrangeVec(D df, I vec) {
        const FixedTag<uint16_t, 4> du16x4;
        const FixedTag<uint32_t, 4> du32x4;
        const FixedTag<float, 4> df32x4;
        Rebind<int32_t, decltype(df32x4)> ru32;
        using VU32x4 = Vec<decltype(df32x4)>;
        const VU32x4 mult255 = ApproximateReciprocal(Set(df32x4, 255));
        return Combine(df, DemoteTo(du16x4, ConvertTo(ru32, Round(Mul(ConvertTo(df32x4,
                                                                                PromoteUpperTo(
                                                                                        du32x4,
                                                                                        vec)),
                                                                      mult255)))),
                       DemoteTo(du16x4, ConvertTo(ru32, Round(Mul(ConvertTo(df32x4,
                                                                            PromoteLowerTo(
                                                                                    du32x4,
                                                                                    vec)),
                                                                  mult255)))));
    }

    void UnpremultiplyRGBA_HWY(const uint8_t *src, int srcStride,
                               uint8_t *dst, int dstStride, int width,
                               int height) {
        const FixedTag<uint8_t, 16> du8x16;
        const FixedTag<uint16_t, 8> du16x8;
        const FixedTag<uint8_t, 8> du8x8;

        using VU8x16 = Vec<decltype(du8x16)>;
        using VU16x8 = Vec<decltype(du16x8)>;

        VU16x8 mult255 = Set(du16x8, 255);

        for (int y = 0; y < height; ++y) {
            auto mSrc = reinterpret_cast<const uint8_t *>(src + y * srcStride);
            auto mDst = reinterpret_cast<uint8_t *>(dst + y * dstStride);

            int x = 0;
            int pixels = 16;

            for (; x + pixels < width; x += pixels) {
                VU8x16 r8, g8, b8, a8;
                LoadInterleaved4(du8x16, mSrc, r8, g8, b8, a8);

                VU16x8 aLow = PromoteLowerTo(du16x8, a8);
                VU16x8 rLow = PromoteLowerTo(du16x8, r8);
                VU16x8 gLow = PromoteLowerTo(du16x8, g8);
                VU16x8 bLow = PromoteLowerTo(du16x8, b8);
                auto lowADivider = ShiftRight<1>(aLow);
                VU16x8 tmp = Add(Mul(Min(rLow, aLow), mult255), lowADivider);
                rLow = RearrangeVec(du16x8, tmp);
                tmp = Add(Mul(Min(gLow, aLow), mult255), lowADivider);
                gLow = RearrangeVec(du16x8, tmp);
                tmp = Add(Mul(Min(bLow, aLow), mult255), lowADivider);
                bLow = RearrangeVec(du16x8, tmp);

                VU16x8 aHigh = PromoteUpperTo(du16x8, a8);
                VU16x8 rHigh = PromoteUpperTo(du16x8, r8);
                VU16x8 gHigh = PromoteUpperTo(du16x8, g8);
                VU16x8 bHigh = PromoteUpperTo(du16x8, b8);
                auto highADivider = ShiftRight<1>(aHigh);
                tmp = Add(Mul(Min(rHigh, aHigh), mult255), highADivider);
                rHigh = RearrangeVec(du16x8, tmp);
                tmp = Add(Mul(Min(gHigh, aHigh), mult255), highADivider);
                gHigh = RearrangeVec(du16x8, tmp);
                tmp = Add(Mul(Min(bHigh, aHigh), mult255), highADivider);
                bHigh = RearrangeVec(du16x8, tmp);

                r8 = Combine(du8x16, DemoteTo(du8x8, rHigh), DemoteTo(du8x8, rLow));
                g8 = Combine(du8x16, DemoteTo(du8x8, gHigh), DemoteTo(du8x8, gLow));
                b8 = Combine(du8x16, DemoteTo(du8x8, bHigh), DemoteTo(du8x8, bLow));

                StoreInterleaved4(r8, g8, b8, a8, du8x16, mDst);

                mSrc += pixels * 4;
                mDst += pixels * 4;
            }

            for (; x < width; ++x) {
                uint8_t alpha = mSrc[3];
                mDst[0] = (min(mSrc[0], alpha) * 255 + alpha / 2) / alpha;
                mDst[1] = (min(mSrc[1], alpha) * 255 + alpha / 2) / alpha;
                mDst[2] = (min(mSrc[2], alpha) * 255 + alpha / 2) / alpha;
                mDst[3] = alpha;
                mSrc += 4;
                mDst += 4;
            }
        }
    }

    void PremultiplyRGBA_HWY(const uint8_t *src, int srcStride,
                             uint8_t *dst, int dstStride, int width,
                             int height) {
        const FixedTag<uint8_t, 16> du8x16;
        const FixedTag<uint16_t, 8> du16x8;
        const FixedTag<uint8_t, 8> du8x8;

        using VU8x16 = Vec<decltype(du8x16)>;
        using VU16x8 = Vec<decltype(du16x8)>;

        VU16x8 mult255d2 = Set(du16x8, 255 / 2);

        for (int y = 0; y < height; ++y) {
            auto mSrc = reinterpret_cast<const uint8_t *>(src + y * srcStride);
            auto mDst = reinterpret_cast<uint8_t *>(dst + y * dstStride);

            int x = 0;
            int pixels = 16;

            for (; x + pixels < width; x += pixels) {
                VU8x16 r8, g8, b8, a8;
                LoadInterleaved4(du8x16, mSrc, r8, g8, b8, a8);

                VU16x8 aLow = PromoteLowerTo(du16x8, a8);
                VU16x8 rLow = PromoteLowerTo(du16x8, r8);
                VU16x8 gLow = PromoteLowerTo(du16x8, g8);
                VU16x8 bLow = PromoteLowerTo(du16x8, b8);
                VU16x8 tmp = Add(Mul(rLow, aLow), mult255d2);
                rLow = RearrangeVec(du16x8, tmp);
                tmp = Add(Mul(gLow, aLow), mult255d2);
                gLow = RearrangeVec(du16x8, tmp);
                tmp = Add(Mul(bLow, aLow), mult255d2);
                bLow = RearrangeVec(du16x8, tmp);

                VU16x8 aHigh = PromoteUpperTo(du16x8, a8);
                VU16x8 rHigh = PromoteUpperTo(du16x8, r8);
                VU16x8 gHigh = PromoteUpperTo(du16x8, g8);
                VU16x8 bHigh = PromoteUpperTo(du16x8, b8);
                tmp = Add(Mul(rHigh, aHigh), mult255d2);
                rHigh = RearrangeVec(du16x8, tmp);
                tmp = Add(Mul(gHigh, aHigh), mult255d2);
                gHigh = RearrangeVec(du16x8, tmp);
                tmp = Add(Mul(bHigh, aHigh), mult255d2);
                bHigh = RearrangeVec(du16x8, tmp);

                r8 = Combine(du8x16, DemoteTo(du8x8, rHigh), DemoteTo(du8x8, rLow));
                g8 = Combine(du8x16, DemoteTo(du8x8, gHigh), DemoteTo(du8x8, gLow));
                b8 = Combine(du8x16, DemoteTo(du8x8, bHigh), DemoteTo(du8x8, bLow));

                StoreInterleaved4(r8, g8, b8, a8, du8x16, mDst);

                mSrc += pixels * 4;
                mDst += pixels * 4;
            }

            for (; x < width; ++x) {
                uint8_t alpha = mSrc[3];
                mDst[0] = (mSrc[0] * alpha + 127) / 255;
                mDst[1] = (mSrc[1] * alpha + 127) / 255;
                mDst[2] = (mSrc[2] * alpha + 127) / 255;
                mDst[3] = alpha;
                mSrc += 4;
                mDst += 4;
            }
        }
    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(UnpremultiplyRGBA_HWY);
    HWY_EXPORT(PremultiplyRGBA_HWY);

    HWY_DLLEXPORT void UnpremultiplyRGBA(const uint8_t *src, int srcStride,
                                         uint8_t *dst, int dstStride, int width,
                                         int height) {
        HWY_DYNAMIC_DISPATCH(UnpremultiplyRGBA_HWY)(src, srcStride, dst, dstStride, width, height);
    }

    HWY_DLLEXPORT void PremultiplyRGBA(const uint8_t *src, int srcStride,
                                       uint8_t *dst, int dstStride, int width,
                                       int height) {
        HWY_DYNAMIC_DISPATCH(PremultiplyRGBA_HWY)(src, srcStride, dst, dstStride, width, height);
    }
}
#endif