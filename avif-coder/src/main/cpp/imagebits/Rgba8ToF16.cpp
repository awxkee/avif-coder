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
using hwy::HWY_NAMESPACE::PromoteLowerTo;
using hwy::HWY_NAMESPACE::PromoteUpperTo;
using hwy::HWY_NAMESPACE::Round;
using hwy::HWY_NAMESPACE::LoadInterleaved4;
using hwy::HWY_NAMESPACE::StoreInterleaved4;
using hwy::HWY_NAMESPACE::Store;
using hwy::HWY_NAMESPACE::Add;
using hwy::HWY_NAMESPACE::Div;
using hwy::HWY_NAMESPACE::Load;
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

template<typename D, typename I = Vec<D>>
inline __attribute__((flatten)) I
AttenuateVecR8toBF16(D d, I vec, I alpha) {
  const FixedTag<uint32_t, 4> du32x4;
  const FixedTag<uint16_t, 8> du16x8;
  const FixedTag<uint8_t, 8> du8x8;
  const FixedTag<uint8_t, 4> du8x4;
  const FixedTag<float, 4> df32x4;
  using VF32x4 = Vec<decltype(df32x4)>;
  const VF32x4 mult255 = ApproximateReciprocal(Set(df32x4, 255));

  auto vecLow = LowerHalf(vec);
  auto alphaLow = LowerHalf(alpha);
  auto vk = ConvertTo(df32x4, PromoteLowerTo(du32x4, PromoteTo(du16x8, vecLow)));
  auto mul = ConvertTo(df32x4, PromoteLowerTo(du32x4, PromoteTo(du16x8, alphaLow)));
  vk = Round(Mul(Mul(vk, mul), mult255));
  auto lowlow = DemoteTo(du8x4, ConvertTo(du32x4, vk));

  vk = ConvertTo(df32x4, PromoteUpperTo(du32x4, PromoteTo(du16x8, vecLow)));
  mul = ConvertTo(df32x4, PromoteUpperTo(du32x4, PromoteTo(du16x8, alphaLow)));
  vk = Round(Mul(Mul(vk, mul), mult255));
  auto lowhigh = DemoteTo(du8x4, ConvertTo(du32x4, vk));

  auto vecHigh = UpperHalf(du8x8, vec);
  auto alphaHigh = UpperHalf(du8x8, alpha);

  vk = ConvertTo(df32x4, PromoteLowerTo(du32x4, PromoteTo(du16x8, vecHigh)));
  mul = ConvertTo(df32x4, PromoteLowerTo(du32x4, PromoteTo(du16x8, alphaHigh)));
  vk = Round(Mul(Mul(vk, mul), mult255));
  auto highlow = DemoteTo(du8x4, ConvertTo(du32x4, vk));

  vk = ConvertTo(df32x4, PromoteUpperTo(du32x4, PromoteTo(du16x8, vecHigh)));
  mul = ConvertTo(df32x4, PromoteUpperTo(du32x4, PromoteTo(du16x8, alphaHigh)));
  vk = Round(Mul(Mul(vk, mul), mult255));
  auto highhigh = DemoteTo(du8x4, ConvertTo(du32x4, vk));
  auto low = Combine(du8x8, lowhigh, lowlow);
  auto high = Combine(du8x8, highhigh, highlow);
  return Combine(d, high, low);
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
      ru8Row = AttenuateVecR8toBF16(du8x16, ru8Row, au8Row);
      gu8Row = AttenuateVecR8toBF16(du8x16, gu8Row, au8Row);
      bu8Row = AttenuateVecR8toBF16(du8x16, bu8Row, au8Row);
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
      r = (r * alpha + 127) / 255;
      g = (g * alpha + 127) / 255;
      b = (b * alpha + 127) / 255;
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

  concurrency::parallel_for(2, height, [&](int y) {
    Rgba8ToF16HWYRow(reinterpret_cast<const uint8_t *>(mSrc + y * srcStride),
                     reinterpret_cast<uint16_t *>(mDst + y * dstStride), width,
                     scale, permuteMap, attenuateAlpha);
  });

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