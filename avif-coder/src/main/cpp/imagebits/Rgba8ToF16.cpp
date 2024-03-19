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
#include <thread>
#include <vector>
#include "half.hpp"
#include "concurrency.hpp"

using namespace std;
using namespace half_float;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "imagebits/Rgba8ToF16.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "attenuate-inl.h"

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

using namespace hwy::HWY_NAMESPACE;
using hwy::float16_t;
using hwy::float32_t;

template<typename D, typename T = Vec<D>>
inline __attribute__((flatten)) T
ConvertRow(D d, T v, const Vec<FixedTag<float32_t, 4>> vScale) {
  FixedTag<int32_t, 4> di32x4;
  Rebind<float32_t, decltype(di32x4)> df32;
  Rebind<float16_t, decltype(df32)> dff16;
  Rebind<uint16_t, decltype(dff16)> du16;

  auto lower = BitCast(du16, DemoteTo(dff16,
                                      Mul(ConvertTo(df32, PromoteLowerTo(di32x4, v)),
                                          vScale)));
  auto upper = BitCast(du16, DemoteTo(dff16,
                                      Mul(ConvertTo(df32,
                                                    PromoteUpperTo(di32x4, v)),
                                          vScale)));
  return Combine(d, upper, lower);
}

void
Rgba8ToF16HWYRow(const uint8_t *source, uint16_t *destination, int width, const float scale,
                 const int *permuteMap, const bool attenuateAlpha) {
  const FixedTag<uint16_t, 8> du16;
  const FixedTag<uint8_t, 16> du8x16;
  using VU16 = Vec<decltype(du16)>;
  using VU8x16 = Vec<decltype(du8x16)>;

  int x = 0;
  const int pixels = 16;

  auto src = reinterpret_cast<const uint8_t *>(source);
  auto dst = reinterpret_cast<uint16_t *>(destination);

  const FixedTag<float32_t, 4> df32x4;
  const auto vScale = Set(df32x4, scale);

  const int permute0Value = permuteMap[0];
  const int permute1Value = permuteMap[1];
  const int permute2Value = permuteMap[2];
  const int permute3Value = permuteMap[3];

  for (; x + pixels < width; x += pixels) {
    VU8x16 ru8Row;
    VU8x16 gu8Row;
    VU8x16 bu8Row;
    VU8x16 au8Row;
    LoadInterleaved4(du8x16, reinterpret_cast<const uint8_t *>(src),
                     ru8Row, gu8Row, bu8Row, au8Row);

    if (attenuateAlpha) {
      ru8Row = AttenuateVec(du8x16, ru8Row, au8Row);
      gu8Row = AttenuateVec(du8x16, gu8Row, au8Row);
      bu8Row = AttenuateVec(du8x16, bu8Row, au8Row);
    }

    auto r16Row = ConvertRow(du16, PromoteLowerTo(du16, ru8Row), vScale);
    auto g16Row = ConvertRow(du16, PromoteLowerTo(du16, gu8Row), vScale);
    auto b16Row = ConvertRow(du16, PromoteLowerTo(du16, bu8Row), vScale);
    auto a16Row = ConvertRow(du16, PromoteLowerTo(du16, au8Row), vScale);

    VU16 pixelsStore[4] = {r16Row, g16Row, b16Row, a16Row};
    VU16 AV = pixelsStore[permute0Value];
    VU16 RV = pixelsStore[permute1Value];
    VU16 GV = pixelsStore[permute2Value];
    VU16 BV = pixelsStore[permute3Value];

    StoreInterleaved4(AV, RV, GV, BV, du16, dst);

    r16Row = ConvertRow(du16, PromoteUpperTo(du16, ru8Row), vScale);
    g16Row = ConvertRow(du16, PromoteUpperTo(du16, gu8Row), vScale);
    b16Row = ConvertRow(du16, PromoteUpperTo(du16, bu8Row), vScale);
    a16Row = ConvertRow(du16, PromoteUpperTo(du16, au8Row), vScale);

    VU16 pixelsStore1[4] = {r16Row, g16Row, b16Row, a16Row};
    AV = pixelsStore1[permute0Value];
    RV = pixelsStore1[permute1Value];
    GV = pixelsStore1[permute2Value];
    BV = pixelsStore1[permute3Value];

    StoreInterleaved4(AV, RV, GV, BV, du16, dst + 8 * 4);

    src += 4 * pixels;
    dst += 4 * pixels;
  }

  for (; x < width; ++x) {
    uint8_t alpha = src[3];
    uint8_t r = src[0];
    uint8_t g = src[1];
    uint8_t b = src[2];

    if (attenuateAlpha) {
      r = (static_cast<uint16_t>(r) * static_cast<uint16_t>(alpha)) / static_cast<uint16_t >(255);
      g = (static_cast<uint16_t>(g) * static_cast<uint16_t>(alpha)) / static_cast<uint16_t >(255);
      b = (static_cast<uint16_t>(b) * static_cast<uint16_t>(alpha)) / static_cast<uint16_t >(255);
    }

    uint16_t tmpR = (uint16_t) half(static_cast<float>(r) * scale).data_;
    uint16_t tmpG = (uint16_t) half(static_cast<float>(g) * scale).data_;
    uint16_t tmpB = (uint16_t) half(static_cast<float>(b) * scale).data_;
    uint16_t tmpA = (uint16_t) half(static_cast<float>(alpha) * scale).data_;

    uint16_t clr[4] = {tmpR, tmpG, tmpB, tmpA};

    dst[0] = clr[permute0Value];
    dst[1] = clr[permute1Value];
    dst[2] = clr[permute2Value];
    dst[3] = clr[permute3Value];

    src += 4;
    dst += 4;
  }
}

void Rgba8ToF16HWY(const uint8_t *sourceData, int srcStride,
                   uint16_t *dst, int dstStride, int width,
                   int height, int bitDepth, const int *permuteMap, const bool attenuateAlpha) {
  auto mSrc = reinterpret_cast<const uint8_t *>(sourceData);
  auto mDst = reinterpret_cast<uint8_t *>(dst);

  const float scale = 1.0f / float(std::powf(2.f, bitDepth) - 1.f);

  for (uint32_t y = 0; y < height; ++y) {
    Rgba8ToF16HWYRow(reinterpret_cast<const uint8_t *>(mSrc),
                     reinterpret_cast<uint16_t *>(mDst), width,
                     scale, permuteMap, attenuateAlpha);

    mSrc += srcStride;
    mDst += dstStride;
  }
}
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
HWY_EXPORT(Rgba8ToF16HWY);
HWY_DLLEXPORT void Rgba8ToF16(const uint8_t *sourceData, int srcStride,
                              uint16_t *dst, int dstStride, int width,
                              int height, const bool attenuateAlpha) {
  int permuteMap[4] = {0, 1, 2, 3};
  HWY_DYNAMIC_DISPATCH(Rgba8ToF16HWY)(sourceData, srcStride, dst, dstStride, width,
                                      height, 8, permuteMap, attenuateAlpha);
}
}
#endif