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
#include "fast_math-inl.h"

#define HWY_ATTENUATE_INLINE inline __attribute__((flatten))

namespace coder::HWY_NAMESPACE {
HWY_BEFORE_NAMESPACE();

using namespace hwy::HWY_NAMESPACE;
using hwy::EnableIf;
using hwy::IsSame;
using hwy::float16_t;
using hwy::float32_t;

template<typename D, typename V = Vec<D>, HWY_IF_U8_D(D), HWY_IF_LANES_D(D, 16)>
HWY_ATTENUATE_INLINE V
AttenuateVec(D d, V vec, V alpha) {
  const Half<decltype(d)> dh;
  const Rebind<uint16_t, decltype(dh)> du16x8;
  using VU16x8 = Vec<decltype(du16x8)>;
  VU16x8 rh = PromoteUpperTo(du16x8, vec);
  VU16x8 ah = PromoteUpperTo(du16x8, alpha);
  rh = DivBy255(du16x8, Mul(rh, ah));
  VU16x8 rl = PromoteLowerTo(du16x8, alpha);
  VU16x8 al = PromoteLowerTo(du16x8, vec);
  rl = DivBy255(du16x8, Mul(rl, al));
  return Combine(d, DemoteTo(dh, rh), DemoteTo(dh, rl));
}

template<class D, typename V = Vec<D>, HWY_IF_U8_D(D), HWY_IF_LANES_D(D, 8)>
HWY_ATTENUATE_INLINE V
AttenuateVec(D d, V vec, V alpha) {
  const Rebind<uint16_t, decltype(d)> du16x8;
  using VU16x8 = Vec<decltype(du16x8)>;
  VU16x8 rh = PromoteTo(du16x8, vec);
  VU16x8 ah = PromoteTo(du16x8, alpha);
  rh = DivBy255(du16x8, Mul(rh, ah));
  return DemoteTo(d, rh);
}

template<class D, typename V = Vec<D>, HWY_IF_U8_D(D), HWY_IF_LANES_D(D, 4)>
HWY_ATTENUATE_INLINE V
AttenuateVec(D d, V vec, V alpha) {
  const Rebind<uint16_t, decltype(d)> du16x8;
  using VU16x8 = Vec<decltype(du16x8)>;
  VU16x8 rh = PromoteTo(du16x8, vec);
  VU16x8 ah = PromoteTo(du16x8, alpha);
  rh = DivBy255(du16x8, Mul(rh, ah));
  return DemoteTo(d, rh);
}
}

HWY_AFTER_NAMESPACE();

#endif