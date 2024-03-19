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
#include "concurrency.hpp"

using namespace std;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "imagebits/RGBAlpha.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "fast_math-inl.h"

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

using namespace hwy;
using namespace hwy::HWY_NAMESPACE;

HWY_INLINE HWY_FLATTEN Vec<FixedTag<uint16_t, 8>>
RearrangeVecAlpha(Vec<FixedTag<uint16_t, 8>> vec, Vec<FixedTag<uint16_t, 8>> alphas) {
  const FixedTag<uint16_t, 8> du16x8;
  const FixedTag<uint16_t, 4> du16x4;
  const FixedTag<uint32_t, 4> du32x4;
  const FixedTag<float, 4> df32x4;
  Rebind<int32_t, decltype(df32x4)> ru32;
  using VF32 = Vec<decltype(df32x4)>;
  const auto ones = Set(df32x4, 1.0f);
  const auto zeros = Zero(df32x4);
  VF32 lowDiv = ConvertTo(df32x4, PromoteLowerTo(du32x4, alphas));
  VF32 highDiv = ConvertTo(df32x4, PromoteUpperTo(du32x4, alphas));
  lowDiv = IfThenElse(lowDiv == zeros, ones, lowDiv);
  highDiv = IfThenElse(highDiv == zeros, ones, highDiv);
  return Combine(du16x8, DemoteTo(du16x4, ConvertTo(ru32, Round(Div(ConvertTo(df32x4,
                                                                              PromoteUpperTo(
                                                                                  du32x4,
                                                                                  vec)),
                                                                    highDiv)))),
                 DemoteTo(du16x4, ConvertTo(ru32, Round(Div(ConvertTo(df32x4,
                                                                      PromoteLowerTo(
                                                                          du32x4,
                                                                          vec)),
                                                            lowDiv)))));
}

void UnpremultiplyRGBA_HWY(const uint8_t *src, int srcStride,
                           uint8_t *dst, int dstStride, int width,
                           int height) {
  concurrency::parallel_for(2, height, [&](int y) {
    const FixedTag<uint8_t, 16> du8x16;
    const FixedTag<uint16_t, 8> du16x8;
    const FixedTag<uint8_t, 8> du8x8;

    using VU8x16 = Vec<decltype(du8x16)>;
    using VU16x8 = Vec<decltype(du16x8)>;

    VU16x8 mult255 = Set(du16x8, 255);

    auto mSrc = reinterpret_cast<const uint8_t *>(src + y * srcStride);
    auto mDst = reinterpret_cast<uint8_t *>(dst + y * dstStride);

    int x = 0;
    const int pixels = 16;

    for (; x + pixels < width; x += pixels) {
      VU8x16 r8, g8, b8, a8;
      LoadInterleaved4(du8x16, mSrc, r8, g8, b8, a8);

      VU16x8 aLow = PromoteLowerTo(du16x8, a8);
      VU16x8 rLow = PromoteLowerTo(du16x8, r8);
      VU16x8 gLow = PromoteLowerTo(du16x8, g8);
      VU16x8 bLow = PromoteLowerTo(du16x8, b8);
      auto lowADivider = ShiftRight<1>(aLow);
      VU16x8 tmp = Add(Mul(rLow, mult255), lowADivider);
      rLow = RearrangeVecAlpha(tmp, aLow);
      tmp = Add(Mul(gLow, mult255), lowADivider);
      gLow = RearrangeVecAlpha(tmp, aLow);
      tmp = Add(Mul(bLow, mult255), lowADivider);
      bLow = RearrangeVecAlpha(tmp, aLow);

      VU16x8 aHigh = PromoteUpperTo(du16x8, a8);
      VU16x8 rHigh = PromoteUpperTo(du16x8, r8);
      VU16x8 gHigh = PromoteUpperTo(du16x8, g8);
      VU16x8 bHigh = PromoteUpperTo(du16x8, b8);
      auto highADivider = ShiftRight<1>(aHigh);
      tmp = Add(Mul(rHigh, mult255), highADivider);
      rHigh = RearrangeVecAlpha(tmp, aHigh);
      tmp = Add(Mul(gHigh, mult255), highADivider);
      gHigh = RearrangeVecAlpha(tmp, aHigh);
      tmp = Add(Mul(bHigh, mult255), highADivider);
      bHigh = RearrangeVecAlpha(tmp, aHigh);

      r8 = Combine(du8x16, DemoteTo(du8x8, rHigh), DemoteTo(du8x8, rLow));
      g8 = Combine(du8x16, DemoteTo(du8x8, gHigh), DemoteTo(du8x8, gLow));
      b8 = Combine(du8x16, DemoteTo(du8x8, bHigh), DemoteTo(du8x8, bLow));

      StoreInterleaved4(r8, g8, b8, a8, du8x16, mDst);

      mSrc += pixels * 4;
      mDst += pixels * 4;
    }

    for (; x < width; ++x) {
      uint8_t alpha = mSrc[3];

      if (alpha != 0) {
        mDst[0] = (static_cast<uint16_t >(mSrc[0]) * static_cast<uint16_t >(255)
            + static_cast<uint16_t>(alpha >> 1)) / static_cast<uint16_t >(alpha);
        mDst[1] = (static_cast<uint16_t >(mSrc[1]) * static_cast<uint16_t >(255)
            + static_cast<uint16_t>(alpha >> 1)) / static_cast<uint16_t >(alpha);
        mDst[2] = (static_cast<uint16_t >(mSrc[2]) * static_cast<uint16_t >(255)
            + static_cast<uint16_t>(alpha >> 1)) / static_cast<uint16_t >(alpha);
      } else {
        mDst[0] = 0;
        mDst[1] = 0;
        mDst[2] = 0;
      }
      mDst[3] = alpha;
      mSrc += 4;
      mDst += 4;
    }
  });
}

void PremultiplyRGBA_HWY(const uint8_t *src, int srcStride,
                         uint8_t *dst, int dstStride, int width,
                         int height) {
  for (uint32_t y = 0; y < height; ++y) {
    const FixedTag<uint8_t, 16> du8x16;
    const FixedTag<uint16_t, 8> du16x8;
    const FixedTag<uint8_t, 8> du8x8;

    using VU8x16 = Vec<decltype(du8x16)>;
    using VU16x8 = Vec<decltype(du16x8)>;

    auto mSrc = reinterpret_cast<const uint8_t *>(src);
    auto mDst = reinterpret_cast<uint8_t *>(dst);

    int x = 0;
    const int pixels = 16;

    for (; x + pixels < width; x += pixels) {
      VU8x16 r8, g8, b8, a8;
      LoadInterleaved4(du8x16, mSrc, r8, g8, b8, a8);

      VU16x8 rh = PromoteUpperTo(du16x8, r8);
      VU16x8 gh = PromoteUpperTo(du16x8, g8);
      VU16x8 bh = PromoteUpperTo(du16x8, b8);
      VU16x8 ah = PromoteUpperTo(du16x8, a8);

      rh = DivBy255(du16x8, Mul(rh, ah));
      gh = DivBy255(du16x8, Mul(gh, ah));
      bh = DivBy255(du16x8, Mul(bh, ah));

      VU16x8 rl = PromoteLowerTo(du16x8, r8);
      VU16x8 gl = PromoteLowerTo(du16x8, g8);
      VU16x8 bl = PromoteLowerTo(du16x8, b8);
      VU16x8 al = PromoteLowerTo(du16x8, a8);

      rl = DivBy255(du16x8, Mul(rl, al));
      gl = DivBy255(du16x8, Mul(gl, al));
      bl = DivBy255(du16x8, Mul(bl, al));

      r8 = Combine(du8x16, DemoteTo(du8x8, rh), DemoteTo(du8x8, rl));
      g8 = Combine(du8x16, DemoteTo(du8x8, gh), DemoteTo(du8x8, gl));
      b8 = Combine(du8x16, DemoteTo(du8x8, bh), DemoteTo(du8x8, bl));

      StoreInterleaved4(r8, g8, b8, a8, du8x16, mDst);

      mSrc += pixels * 4;
      mDst += pixels * 4;
    }

    for (; x < width; ++x) {
      uint8_t alpha = mSrc[3];
      mDst[0] = (static_cast<uint16_t>(mSrc[0]) * static_cast<uint16_t>(alpha))
          / static_cast<uint16_t >(255);
      mDst[1] = (static_cast<uint16_t>(mSrc[1]) * static_cast<uint16_t>(alpha))
          / static_cast<uint16_t >(255);
      mDst[2] = (static_cast<uint16_t>(mSrc[2]) * static_cast<uint16_t>(alpha))
          / static_cast<uint16_t >(255);
      mDst[3] = alpha;
      mSrc += 4;
      mDst += 4;
    }

    src += srcStride;
    dst += dstStride;
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