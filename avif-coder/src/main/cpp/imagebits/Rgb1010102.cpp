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

#include "Rgb1010102.h"
#include <vector>
#include <thread>
#include <algorithm>
#include "half.hpp"
#include "concurrency.hpp"

using namespace std;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "imagebits/Rgb1010102.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

using hwy::HWY_NAMESPACE::FixedTag;
using hwy::HWY_NAMESPACE::Rebind;
using hwy::HWY_NAMESPACE::Vec;
using hwy::HWY_NAMESPACE::Set;
using hwy::HWY_NAMESPACE::Zero;
using hwy::HWY_NAMESPACE::Load;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::Max;
using hwy::HWY_NAMESPACE::Min;
using hwy::HWY_NAMESPACE::And;
using hwy::HWY_NAMESPACE::Add;
using hwy::HWY_NAMESPACE::BitCast;
using hwy::HWY_NAMESPACE::ShiftRight;
using hwy::HWY_NAMESPACE::ShiftLeft;
using hwy::HWY_NAMESPACE::ExtractLane;
using hwy::HWY_NAMESPACE::DemoteTo;
using hwy::HWY_NAMESPACE::ConvertTo;
using hwy::HWY_NAMESPACE::PromoteTo;
using hwy::HWY_NAMESPACE::LoadInterleaved4;
using hwy::HWY_NAMESPACE::Or;
using hwy::HWY_NAMESPACE::ShiftLeft;
using hwy::HWY_NAMESPACE::Store;
using hwy::HWY_NAMESPACE::Round;
using hwy::HWY_NAMESPACE::LowerHalf;
using hwy::HWY_NAMESPACE::UpperHalf;
using hwy::HWY_NAMESPACE::PromoteLowerTo;
using hwy::HWY_NAMESPACE::PromoteUpperTo;
using hwy::float16_t;
using hwy::float32_t;

void
F16ToRGBA1010102HWYRow(const uint16_t *HWY_RESTRICT data, uint32_t *HWY_RESTRICT dst,
                       int width,
                       const int *permuteMap) {
  const float range10 = std::powf(2.0f, 10.0f) - 1.0f;
  const FixedTag<float, 4> df;
  const FixedTag<float16_t, 8> df16;
  const FixedTag<uint16_t, 8> du16x8;
  const Rebind<int32_t, FixedTag<float, 4>> di32;
  const FixedTag<uint32_t, 4> du;
  using V = Vec<decltype(df)>;
  using V16 = Vec<decltype(df16)>;
  using VU16x8 = Vec<decltype(du16x8)>;
  using VU = Vec<decltype(du)>;
  const auto vRange10 = Set(df, range10);
  const auto zeros = Zero(df);
  const auto alphaMax = Set(df, 3.0);

  const Rebind<float16_t, FixedTag<uint16_t, 8>> rbfu16;
  const Rebind<float32_t, FixedTag<float16_t, 4>> rbf32;

  int x = 0;
  auto dst32 = reinterpret_cast<uint32_t *>(dst);
  int pixels = 8;
  const auto avShift = permuteMap[0];
  const auto rvShift = permuteMap[1];
  const auto gvShift = permuteMap[2];
  const auto bvShift = permuteMap[3];
  for (; x + pixels < width; x += pixels) {
    VU16x8 upixels1;
    VU16x8 upixels2;
    VU16x8 upixels3;
    VU16x8 upixels4;
    LoadInterleaved4(du16x8,
                     reinterpret_cast<const uint16_t *>(data),
                     upixels1,
                     upixels2,
                     upixels3,
                     upixels4);

    V16 fpixels1 = BitCast(rbfu16, upixels1);
    V16 fpixels2 = BitCast(rbfu16, upixels2);
    V16 fpixels3 = BitCast(rbfu16, upixels3);
    V16 fpixels4 = BitCast(rbfu16, upixels4);

    V pixelsLow1 = Min(
        Max(Round(Mul(PromoteLowerTo(rbf32, fpixels1), vRange10)), zeros),
        vRange10);
    V pixelsLow2 = Min(
        Max(Round(Mul(PromoteLowerTo(rbf32, fpixels2), vRange10)), zeros),
        vRange10);
    V pixelsLow3 = Min(
        Max(Round(Mul(PromoteLowerTo(rbf32, fpixels3), vRange10)), zeros),
        vRange10);
    V pixelsLow4 = Min(
        Max(Round(Mul(PromoteLowerTo(rbf32, fpixels4), alphaMax)), zeros),
        alphaMax);

    VU pixelsuLow1 = BitCast(du, ConvertTo(di32, pixelsLow1));
    VU pixelsuLow2 = BitCast(du, ConvertTo(di32, pixelsLow2));
    VU pixelsuLow3 = BitCast(du, ConvertTo(di32, pixelsLow3));
    VU pixelsuLow4 = BitCast(du, ConvertTo(di32, pixelsLow4));

    V pixelsHigh1 = Min(
        Max(Round(Mul(PromoteUpperTo(rbf32, fpixels1), vRange10)), zeros),
        vRange10);
    V pixelsHigh2 = Min(
        Max(Round(Mul(PromoteUpperTo(rbf32, fpixels2), vRange10)), zeros),
        vRange10);
    V pixelsHigh3 = Min(
        Max(Round(Mul(PromoteUpperTo(rbf32, fpixels3), vRange10)), zeros),
        vRange10);
    V pixelsHigh4 = Min(
        Max(Round(Mul(PromoteUpperTo(rbf32, fpixels4), alphaMax)), zeros),
        alphaMax);
    VU pixelsuHigh1 = BitCast(du, ConvertTo(di32, pixelsHigh1));
    VU pixelsuHigh2 = BitCast(du, ConvertTo(di32, pixelsHigh2));
    VU pixelsuHigh3 = BitCast(du, ConvertTo(di32, pixelsHigh3));
    VU pixelsuHigh4 = BitCast(du, ConvertTo(di32, pixelsHigh4));

    VU pixelsHighStore[4] = {pixelsuHigh1, pixelsuHigh2, pixelsuHigh3, pixelsuHigh4};
    VU AV = pixelsHighStore[avShift];
    VU RV = pixelsHighStore[rvShift];
    VU GV = pixelsHighStore[gvShift];
    VU BV = pixelsHighStore[bvShift];
    VU upper = Or(ShiftLeft<30>(AV), ShiftLeft<20>(RV));
    VU lower = Or(ShiftLeft<10>(GV), BV);
    VU final = Or(upper, lower);
    Store(final, du, dst32 + 4);

    VU pixelsLowStore[4] = {pixelsuLow1, pixelsuLow2, pixelsuLow3, pixelsuLow4};

    AV = pixelsLowStore[avShift];
    RV = pixelsLowStore[rvShift];
    GV = pixelsLowStore[gvShift];
    BV = pixelsLowStore[bvShift];
    upper = Or(ShiftLeft<30>(AV), ShiftLeft<20>(RV));
    lower = Or(ShiftLeft<10>(GV), BV);
    final = Or(upper, lower);
    Store(final, du, dst32);

    data += pixels * 4;
    dst32 += pixels;
  }

  for (; x < width; ++x) {
    auto A16 = (float) LoadHalf(data[avShift]);
    auto R16 = (float) LoadHalf(data[rvShift]);
    auto G16 = (float) LoadHalf(data[gvShift]);
    auto B16 = (float) LoadHalf(data[bvShift]);
    auto R10 = static_cast<uint32_t>(std::clamp(std::roundf(R16 * range10), 0.0f,
                                                (float) range10));
    auto G10 = static_cast<uint32_t>(std::clamp(std::roundf(G16 * range10), 0.0f,
                                                (float) range10));
    auto B10 = static_cast<uint32_t>(std::clamp(std::roundf(B16 * range10), 0.0f,
                                                (float) range10));
    auto A10 = static_cast<uint32_t>(std::clamp(std::roundf(A16 * 3.f), 0.0f, 3.0f));

    dst32[0] = (A10 << 30) | (R10 << 20) | (G10 << 10) | B10;

    data += 4;
    dst32 += 1;
  }
}

template<typename D, typename I = Vec<D>>
inline __attribute__((flatten)) I
AttenuateVecR1010102(D d, I vec, I alpha) {
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

template<typename D, typename R, typename T = Vec<D>, typename I = Vec<R>>
inline __attribute__((flatten)) Vec<D>
ConvertPixelsTo(D d, R rd, I r, I g, I b, I a, const int *permuteMap) {
  using RD = Vec<R>;
  const T MulBy4Const = Set(d, 4);
  const T MulBy3Const = Set(d, 3);
  const T O127Const = Set(d, 127);
  T pixelsu1 = Mul(BitCast(d, r), MulBy4Const);
  T pixelsu2 = Mul(BitCast(d, g), MulBy4Const);
  T pixelsu3 = Mul(BitCast(d, b), MulBy4Const);
  T pixelsu4 = ShiftRight<8>(Add(Mul(BitCast(d, a), MulBy3Const), O127Const));

  const int permute0Value = permuteMap[0];
  const int permute1Value = permuteMap[1];
  const int permute2Value = permuteMap[2];
  const int permute3Value = permuteMap[3];

  T pixelsStore[4] = {pixelsu1, pixelsu2, pixelsu3, pixelsu4};
  T AV = pixelsStore[permute0Value];
  T RV = pixelsStore[permute1Value];
  T GV = pixelsStore[permute2Value];
  T BV = pixelsStore[permute3Value];
  T upper = Or(ShiftLeft<30>(AV), ShiftLeft<20>(RV));
  T lower = Or(ShiftLeft<10>(GV), BV);
  T final = Or(upper, lower);
  return final;
}

void
Rgba8ToRGBA1010102HWYRow(const uint8_t *HWY_RESTRICT data, uint32_t *HWY_RESTRICT dst,
                         int width,
                         const int *permuteMap,
                         const bool attenuateAlpha) {
  const FixedTag<uint8_t, 16> du8x16;
  const FixedTag<uint16_t, 8> du16x4;
  const Rebind<int32_t, FixedTag<uint8_t, 4>> di32;
  const FixedTag<uint32_t, 4> du;
  using VU8x16 = Vec<decltype(du8x16)>;
  using VU = Vec<decltype(du)>;

  const int permute0Value = permuteMap[0];
  const int permute1Value = permuteMap[1];
  const int permute2Value = permuteMap[2];
  const int permute3Value = permuteMap[3];

  int x = 0;
  auto dst32 = reinterpret_cast<uint32_t *>(dst);
  int pixels = 16;
  for (; x + pixels < width; x += pixels) {
    VU8x16 upixels1;
    VU8x16 upixels2;
    VU8x16 upixels3;
    VU8x16 upixels4;
    LoadInterleaved4(du8x16, reinterpret_cast<const uint8_t *>(data),
                     upixels1,
                     upixels2,
                     upixels3,
                     upixels4);

    if (attenuateAlpha) {
      upixels1 = AttenuateVecR1010102(du8x16, upixels1, upixels4);
      upixels2 = AttenuateVecR1010102(du8x16, upixels2, upixels4);
      upixels3 = AttenuateVecR1010102(du8x16, upixels3, upixels4);
    }

    VU final = ConvertPixelsTo(du, di32,
                               PromoteUpperTo(di32, PromoteUpperTo(du16x4, upixels1)),
                               PromoteUpperTo(di32, PromoteUpperTo(du16x4, upixels2)),
                               PromoteUpperTo(di32, PromoteUpperTo(du16x4, upixels3)),
                               PromoteUpperTo(di32, PromoteUpperTo(du16x4, upixels4)),
                               permuteMap);
    StoreU(final, du, dst32 + 12);

    final = ConvertPixelsTo(du, di32,
                            PromoteLowerTo(di32, PromoteUpperTo(du16x4, upixels1)),
                            PromoteLowerTo(di32, PromoteUpperTo(du16x4, upixels2)),
                            PromoteLowerTo(di32, PromoteUpperTo(du16x4, upixels3)),
                            PromoteLowerTo(di32, PromoteUpperTo(du16x4, upixels4)),
                            permuteMap);
    StoreU(final, du, dst32 + 8);

    final = ConvertPixelsTo(du, di32,
                            PromoteUpperTo(di32, PromoteLowerTo(du16x4, upixels1)),
                            PromoteUpperTo(di32, PromoteLowerTo(du16x4, upixels2)),
                            PromoteUpperTo(di32, PromoteLowerTo(du16x4, upixels3)),
                            PromoteUpperTo(di32, PromoteLowerTo(du16x4, upixels4)),
                            permuteMap);
    StoreU(final, du, dst32 + 4);

    final = ConvertPixelsTo(du, di32,
                            PromoteLowerTo(di32, PromoteLowerTo(du16x4, upixels1)),
                            PromoteLowerTo(di32, PromoteLowerTo(du16x4, upixels2)),
                            PromoteLowerTo(di32, PromoteLowerTo(du16x4, upixels3)),
                            PromoteLowerTo(di32, PromoteLowerTo(du16x4, upixels4)),
                            permuteMap);
    StoreU(final, du, dst32);

    data += pixels * 4;
    dst32 += pixels;
  }

  for (; x < width; ++x) {
    uint8_t alpha = data[permute0Value];
    uint8_t r = data[permute1Value];
    uint8_t g = data[permute2Value];
    uint8_t b = data[permute3Value];

    if (attenuateAlpha) {
      r = (r * alpha + 127) / 255;
      g = (g * alpha + 127) / 255;
      b = (b * alpha + 127) / 255;
    }

    auto A16 = ((uint32_t) alpha * 3 + 127) >> 8;
    auto R16 = (uint32_t) r * 4;
    auto G16 = (uint32_t) g * 4;
    auto B16 = (uint32_t) b * 4;

    dst32[0] = (A16 << 30) | (R16 << 20) | (G16 << 10) | B16;
    data += 4;
    dst32 += 1;
  }
}

void
Rgba8ToRGBA1010102HWY(const uint8_t *source,
                      int srcStride,
                      uint8_t *destination,
                      int dstStride,
                      int width,
                      int height,
                      const bool attenuateAlpha) {
  int permuteMap[4] = {3, 2, 1, 0};
  auto src = reinterpret_cast<const uint8_t *>(source);
  auto dst = reinterpret_cast<uint8_t *>(destination);

  concurrency::parallel_for(2, height, [&](int y) {
    Rgba8ToRGBA1010102HWYRow(reinterpret_cast<const uint8_t *>(src + srcStride * y),
                             reinterpret_cast<uint32_t *>(dst + dstStride * y),
                             width, &permuteMap[0], attenuateAlpha);
  });
}

void
F16ToRGBA1010102HWY(const uint16_t *source,
                    int srcStride,
                    uint8_t *destination,
                    int dstStride,
                    int width,
                    int height) {
  int permuteMap[4] = {3, 2, 1, 0};
  auto src = reinterpret_cast<const uint8_t *>(source);
  auto dst = reinterpret_cast<uint8_t *>(destination);

  concurrency::parallel_for(2, height, [&](int y) {
    F16ToRGBA1010102HWYRow(reinterpret_cast<const uint16_t *>(src + srcStride * y),
                           reinterpret_cast<uint32_t *>(dst + dstStride * y),
                           width, &permuteMap[0]);
  });
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace coder {

template<typename V>
void RGBA1010102ToUnsigned(const uint8_t *__restrict__ src, const int srcStride,
                           V *__restrict__ dst, const int dstStride,
                           const int width, const int height, const int bitDepth) {
  auto mDstPointer = reinterpret_cast<uint8_t *>(dst);
  auto mSrcPointer = reinterpret_cast<const uint8_t *>(src);

  const float maxColors = std::powf(2.f, bitDepth) - 1.f;
  const float valueScale = maxColors / 1023.f;
  const float alphaValueScale = maxColors / 3.f;

  const uint32_t mask = (1u << 10u) - 1u;

  for (int y = 0; y < height; ++y) {

    auto dstPointer = reinterpret_cast<V *>(mDstPointer);
    auto srcPointer = reinterpret_cast<const uint8_t *>(mSrcPointer);

    for (int x = 0; x < width; ++x) {
      uint32_t rgba1010102 = reinterpret_cast<const uint32_t *>(srcPointer)[0];

      uint32_t r = (rgba1010102) & mask;
      uint32_t g = (rgba1010102 >> 10) & mask;
      uint32_t b = (rgba1010102 >> 20) & mask;
      uint32_t a1 = (rgba1010102 >> 30);

      V ru = std::clamp(static_cast<V>(std::roundf(r * valueScale)), static_cast<V>(0),
                        static_cast<V>(maxColors));
      V gu = std::clamp(static_cast<V>(std::roundf(g * valueScale)), static_cast<V>(0),
                        static_cast<V>(maxColors));
      V bu = std::clamp(static_cast<V>(std::roundf(b * valueScale)), static_cast<V>(0),
                        static_cast<V>(maxColors));
      V au = std::clamp(static_cast<V>(std::roundf(a1 * alphaValueScale)),
                        static_cast<V>(0), static_cast<V>(maxColors));

      if (std::is_same<V, uint8_t>::value) {
        ru = (ru * au + 127) / 255;
        gu = (gu * au + 127) / 255;
        bu = (bu * au + 127) / 255;
      }

      dstPointer[0] = ru;
      dstPointer[1] = gu;
      dstPointer[2] = bu;
      dstPointer[3] = au;

      srcPointer += 4;
      dstPointer += 4;
    }

    mSrcPointer += srcStride;
    mDstPointer += dstStride;
  }
}

template void RGBA1010102ToUnsigned(const uint8_t *__restrict__ src, const int srcStride,
                                    uint8_t *__restrict__ dst, const int dstStride,
                                    const int width, const int height, const int bitDepth);

template void RGBA1010102ToUnsigned(const uint8_t *__restrict__ src, const int srcStride,
                                    uint16_t *__restrict__ dst, const int dstStride,
                                    const int width, const int height, const int bitDepth);

HWY_EXPORT(F16ToRGBA1010102HWY);
HWY_EXPORT(Rgba8ToRGBA1010102HWY);
HWY_DLLEXPORT void
F16ToRGBA1010102(const uint16_t *source, int srcStride, uint8_t *destination, int dstStride,
                 int width,
                 int height) {
  HWY_DYNAMIC_DISPATCH(F16ToRGBA1010102HWY)(source, srcStride, destination, dstStride, width,
                                            height);
}

HWY_DLLEXPORT void
Rgba8ToRGBA1010102(const uint8_t *source,
                   int srcStride,
                   uint8_t *destination,
                   int dstStride,
                   int width,
                   int height,
                   const bool attenuateAlpha) {
  HWY_DYNAMIC_DISPATCH(Rgba8ToRGBA1010102HWY)(source, srcStride, destination, dstStride,
                                              width,
                                              height, attenuateAlpha);
}
}

#endif