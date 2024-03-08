/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 14/01/2024
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

#if defined(HIGHWAY_HWY_CODER_CMS_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_CODER_CMS_INL_H_
#undef HIGHWAY_HWY_CODER_CMS_INL_H_
#else
#define HIGHWAY_HWY_CODER_CMS_INL_H_
#endif

#include "hwy/highway.h"

#include <vector>
#include "Eigen/Eigen"

// https://64.github.io/tonemapping/
// https://www.russellcottrell.com/photo/matrixCalculator.htm

using namespace std;

#if defined(__clang__)
#pragma clang fp contract(fast) exceptions(ignore) reassociate(on)
#endif

#define HWY_CODER_CMS_INLINE inline __attribute__((flatten))

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

using hwy::EnableIf;
using hwy::IsFloat;
using hwy::HWY_NAMESPACE::Vec;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::MulAdd;
using hwy::HWY_NAMESPACE::LoadU;
using hwy::HWY_NAMESPACE::Set;
using hwy::HWY_NAMESPACE::TFromD;

template<class D, typename V = Vec<D>, HWY_IF_FLOAT(TFromD<D>)>
HWY_CODER_CMS_INLINE void
convertColorProfile(const D df, const Eigen::Matrix3f &conversion, V &r, V &g, V &b) {
  const auto tLane0 = Set(df, static_cast<TFromD<D>>(conversion(0, 0)));
  const auto tLane1 = Set(df, static_cast<TFromD<D>>(conversion(0, 1)));
  const auto tLane2 = Set(df, static_cast<TFromD<D>>(conversion(0, 2)));
  auto newR = MulAdd(r, tLane0, MulAdd(g, tLane1, Mul(b, tLane2)));
  const auto tLane3 = Set(df, static_cast<TFromD<D>>(conversion(1, 0)));
  const auto tLane4 = Set(df, static_cast<TFromD<D>>(conversion(1, 1)));
  const auto tLane5 = Set(df, static_cast<TFromD<D>>(conversion(1, 2)));
  auto newG = MulAdd(r, tLane3, MulAdd(g, tLane4, Mul(b, tLane5)));
  const auto tLane6 = Set(df, static_cast<TFromD<D>>(conversion(2, 0)));
  const auto tLane7 = Set(df, static_cast<TFromD<D>>(conversion(2, 1)));
  const auto tLane8 = Set(df, static_cast<TFromD<D>>(conversion(2, 2)));
  auto newB = MulAdd(r, tLane6, MulAdd(g, tLane7, Mul(b, tLane8)));
  r = newR;
  g = newG;
  b = newB;
}

HWY_CODER_CMS_INLINE void
convertColorProfile(const Eigen::Matrix3f &conversion, float &r, float &g, float &b) {
  const Eigen::Vector3f color = {r, g, b};
  const float newR = conversion.row(0).dot(color);
  const float newG = conversion.row(1).dot(color);
  const float newB = conversion.row(2).dot(color);
  r = newR;
  g = newG;
  b = newB;
}

}

HWY_AFTER_NAMESPACE();

#endif /* Colorspace_h */
