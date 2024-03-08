/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 11/01/2024
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

#if defined(HIGHWAY_HWY_ATTENUATE_INL_H) == defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_ATTENUATE_INL_H
#undef HIGHWAY_HWY_ATTENUATE_INL_H
#else
#define HIGHWAY_HWY_ATTENUATE_INL_H
#endif

#include "hwy/highway.h"
#include "hwy/ops/shared-inl.h"

#define HWY_ATTENUATE_INLINE inline __attribute__((flatten))

namespace coder::HWY_NAMESPACE {
HWY_BEFORE_NAMESPACE();

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
using hwy::HWY_NAMESPACE::And;
using hwy::HWY_NAMESPACE::LoadU;
using hwy::HWY_NAMESPACE::Rebind;
using hwy::HWY_NAMESPACE::StoreInterleaved4;
using hwy::HWY_NAMESPACE::LowerHalf;
using hwy::HWY_NAMESPACE::ShiftLeft;
using hwy::HWY_NAMESPACE::ShiftRight;
using hwy::HWY_NAMESPACE::PromoteLowerTo;
using hwy::HWY_NAMESPACE::PromoteUpperTo;
using hwy::HWY_NAMESPACE::UpperHalf;
using hwy::HWY_NAMESPACE::LoadInterleaved4;
using hwy::HWY_NAMESPACE::StoreU;
using hwy::HWY_NAMESPACE::Or;
using hwy::HWY_NAMESPACE::ApproximateReciprocal;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::Div;
using hwy::HWY_NAMESPACE::TFromV;
using hwy::HWY_NAMESPACE::TFromD;
using hwy::EnableIf;
using hwy::IsSame;
using hwy::float16_t;
using hwy::float32_t;

template<typename D, typename V = Vec<D>, HWY_IF_U8_D(D), HWY_IF_LANES_D(D, 16)>
HWY_ATTENUATE_INLINE V
AttenuateVec(D d, V vec, V alpha) {
  const FixedTag<uint32_t, 4> du32x4;
  const FixedTag<uint16_t, 8> du16x8;
  const FixedTag<uint8_t, 8> du8x8;
  const FixedTag<uint8_t, 4> du8x4;
  const FixedTag<float, 4> df32x4;
  using VF32x4 = Vec<decltype(df32x4)>;
  const VF32x4 mult255 = ApproximateReciprocal(Set(df32x4, 255));

  const auto vecLow = LowerHalf(vec);
  const auto alphaLow = LowerHalf(alpha);
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

template<class D, typename V = Vec<D>, HWY_IF_U8_D(D), HWY_IF_LANES_D(D, 8)>
HWY_ATTENUATE_INLINE V
AttenuateVec(D d, V vec, V alpha) {
  const FixedTag<uint32_t, 4> du32x4;
  const FixedTag<uint16_t, 8> du16x8;
  const FixedTag<uint8_t, 8> du8x8;
  const FixedTag<uint8_t, 4> du8x4;
  const FixedTag<float, 4> df32x4;
  using VF32x4 = Vec<decltype(df32x4)>;
  const VF32x4 mult255 = ApproximateReciprocal(Set(df32x4, 255));

  const auto promotedVec = PromoteTo(du16x8, vec);
  const auto promotedAlpha = PromoteTo(du16x8, alpha);
  auto vk = ConvertTo(df32x4, PromoteLowerTo(du32x4, promotedVec));
  auto mul = ConvertTo(df32x4, PromoteLowerTo(du32x4, promotedAlpha));
  vk = Round(Mul(Mul(vk, mul), mult255));
  auto low = DemoteTo(du8x4, ConvertTo(du32x4, vk));

  vk = ConvertTo(df32x4, PromoteUpperTo(du32x4, promotedVec));
  mul = ConvertTo(df32x4, PromoteUpperTo(du32x4, promotedAlpha));
  vk = Round(Mul(Mul(vk, mul), mult255));
  auto high = DemoteTo(du8x4, ConvertTo(du32x4, vk));

  auto attenuated = Combine(du8x8, high, low);
  return attenuated;
}

template<class D, typename V = Vec<D>, HWY_IF_U8_D(D), HWY_IF_LANES_D(D, 4)>
HWY_ATTENUATE_INLINE V
AttenuateVec(D d, V vec, V alpha) {
  const FixedTag<uint32_t, 4> du32x4;
  const FixedTag<uint16_t, 4> du16x8;
  const FixedTag<uint8_t, 4> du8x8;
  const FixedTag<uint8_t, 4> du8x4;
  const FixedTag<float, 4> df32x4;
  using VF32x4 = Vec<decltype(df32x4)>;
  const VF32x4 mult255 = ApproximateReciprocal(Set(df32x4, 255));

  const auto promotedVec = PromoteTo(du16x8, vec);
  const auto promotedAlpha = PromoteTo(du16x8, alpha);
  auto vk = ConvertTo(df32x4, PromoteLowerTo(du32x4, promotedVec));
  auto mul = ConvertTo(df32x4, PromoteLowerTo(du32x4, promotedAlpha));
  vk = Round(Mul(Mul(vk, mul), mult255));
  auto low = DemoteTo(du8x4, ConvertTo(du32x4, vk));
  return low;
}
}

HWY_AFTER_NAMESPACE();

#endif