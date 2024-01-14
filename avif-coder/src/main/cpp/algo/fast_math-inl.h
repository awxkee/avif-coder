// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Fast SIMD math ops (log2, encoder only, cos, erf for splines)

#if defined(LIB_JXL_BASE_FAST_MATH_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef LIB_JXL_BASE_FAST_MATH_INL_H_
#undef LIB_JXL_BASE_FAST_MATH_INL_H_
#else
#define LIB_JXL_BASE_FAST_MATH_INL_H_
#endif

#include <hwy/highway.h>

#include "common.h"
#include "rational_polynomial-inl.h"

#define HWY_FAST_MATH_INLINE inline __attribute__((flatten))

HWY_BEFORE_NAMESPACE();
namespace coder::HWY_NAMESPACE {

// These templates are not found via ADL.
    using hwy::HWY_NAMESPACE::Abs;
    using hwy::HWY_NAMESPACE::Add;
    using hwy::HWY_NAMESPACE::Eq;
    using hwy::HWY_NAMESPACE::Floor;
    using hwy::HWY_NAMESPACE::Ge;
    using hwy::HWY_NAMESPACE::GetLane;
    using hwy::HWY_NAMESPACE::IfThenElse;
    using hwy::HWY_NAMESPACE::IfThenZeroElse;
    using hwy::HWY_NAMESPACE::Le;
    using hwy::HWY_NAMESPACE::Min;
    using hwy::HWY_NAMESPACE::Mul;
    using hwy::HWY_NAMESPACE::MulAdd;
    using hwy::HWY_NAMESPACE::NegMulAdd;
    using hwy::HWY_NAMESPACE::Rebind;
    using hwy::HWY_NAMESPACE::ShiftLeft;
    using hwy::HWY_NAMESPACE::ShiftRight;
    using hwy::HWY_NAMESPACE::Sub;
    using hwy::HWY_NAMESPACE::Xor;
    using hwy::HWY_NAMESPACE::Vec;
    using hwy::float32_t;

    template<class DF, class V>
    HWY_FAST_MATH_INLINE V TaylorPolyExp(const DF df, V x, const V *coeffs) {
        auto A = MulAdd(coeffs[4], x, coeffs[0]);
        auto B = MulAdd(coeffs[6], x, coeffs[2]);
        auto C = MulAdd(coeffs[5], x, coeffs[1]);
        auto D = MulAdd(coeffs[7], x, coeffs[3]);
        auto x2 = Mul(x, x);
        auto x4 = Mul(x2, x2);
        auto res = MulAdd(MulAdd(D, x2, C), x4, MulAdd(B, x2, A));
        return res;
    }

    template<class DF, class V>
    HWY_FAST_MATH_INLINE V
    Exp2f(const DF df, V x) {
        using VF32 = Vec<decltype(df)>;
        using T = hwy::HWY_NAMESPACE::TFromD<DF>;
        static const VF32 expTab[8] =
                {
                        Set(df, static_cast<T>(1.f)),
                        Set(df, static_cast<T>(0.0416598916054f)),
                        Set(df, static_cast<T>(0.500000596046f)),
                        Set(df, static_cast<T>(0.0014122662833f)),
                        Set(df, static_cast<T>(1.00000011921f)),
                        Set(df, static_cast<T>(0.00833693705499f)),
                        Set(df, static_cast<T>(0.166665703058f)),
                        Set(df, static_cast<T>(0.000195780929062f)),
                };
        Rebind<int32_t, decltype(df)> s32;
        using VS32 = Vec<decltype(s32)>;
        Rebind<float, decltype(s32)> fs32;
        static const VF32 CONST_LN2 = Set(df, static_cast<T>(0.6931471805f)); // ln(2)
        static const VF32 CONST_INV_LN2 = Set(df, static_cast<T>(1.4426950408f)); // 1/ln(2)
        // Perform range reduction [-log(2),log(2)]
        VS32 m = ConvertTo(s32, Mul(x, CONST_INV_LN2));
        auto val = NegMulAdd(ConvertTo(fs32, m), CONST_LN2, x);
        // Polynomial Approximation
        auto poly = TaylorPolyExp(df, val, &expTab[0]);
        // Reconstruct
        poly = BitCast(fs32, Add(BitCast(s32, poly), ShiftLeft<23>(m)));
        return poly;
    }

    template<class DF, class V>
    HWY_FAST_MATH_INLINE V
    Lognf(const DF df, V x) {
        using VF32 = Vec<decltype(df)>;
        using T = hwy::HWY_NAMESPACE::TFromD<DF>;
        Rebind<int32_t, decltype(df)> sb32;
        Rebind<float32_t, decltype(sb32)> fli32;
        static const VF32 logTab[8] =
                {
                        Set(df, static_cast<T>(-2.29561495781f)),
                        Set(df, static_cast<T>(-2.47071170807f)),
                        Set(df, static_cast<T>(-5.68692588806f)),
                        Set(df, static_cast<T>(-0.165253549814f)),
                        Set(df, static_cast<T>(5.17591238022f)),
                        Set(df, static_cast<T>(0.844007015228f)),
                        Set(df, static_cast<T>(4.58445882797f)),
                        Set(df, static_cast<T>(0.0141278216615f)),
                };
        using VS32 = Vec<decltype(sb32)>;
        static const VS32 CONST_127 = Set(sb32, 127);           // 127
        static const VF32 CONST_LN2 = Set(df, static_cast<T>(0.6931471805f)); // ln(2)
        // Extract exponent
        VS32 m = Sub(BitCast(sb32, ShiftRight<23>(BitCast(sb32, BitCast(sb32, x)))), CONST_127);
        VF32 val = BitCast(fli32,
                           Sub(BitCast(sb32, x), ShiftLeft<23>(m)));
        // Polynomial Approximation
        VF32 poly = TaylorPolyExp(df, val, &logTab[0]);
        // Reconstruct
        poly = MulAdd(ConvertTo(fli32, m), CONST_LN2, poly);
        return poly;
    }

    template<class DF, class V>
    HWY_FAST_MATH_INLINE V Pow(const DF df, V val, V n) {
        return Exp2f(df, Mul(n, Lognf(df, val)));
    }

// Computes base-2 logarithm like std::log2. Undefined if negative / NaN.
// L1 error ~3.9E-6
    template<class DF, class V>
    HWY_FAST_MATH_INLINE V FastLog2f(const DF df, V x) {
        // 2,2 rational polynomial approximation of std::log1p(x) / std::log(2).
        HWY_ALIGN const float p[4 * (2 + 1)] = {HWY_REP4(-1.8503833400518310E-06f),
                                                HWY_REP4(1.4287160470083755E+00f),
                                                HWY_REP4(7.4245873327820566E-01f)};
        HWY_ALIGN const float q[4 * (2 + 1)] = {HWY_REP4(9.9032814277590719E-01f),
                                                HWY_REP4(1.0096718572241148E+00f),
                                                HWY_REP4(1.7409343003366853E-01f)};

        const Rebind<int32_t, DF> di;
        const auto x_bits = BitCast(di, x);

        // Range reduction to [-1/3, 1/3] - 3 integer, 2 float ops
        const auto exp_bits = Sub(x_bits, Set(di, 0x3f2aaaab));  // = 2/3
        // Shifted exponent = log2; also used to clear mantissa.
        const auto exp_shifted = ShiftRight<23>(exp_bits);
        const auto mantissa = BitCast(df, Sub(x_bits, ShiftLeft<23>(exp_shifted)));
        const auto exp_val = ConvertTo(df, exp_shifted);
        return Add(EvalRationalPolynomial(df, Sub(mantissa, Set(df, 1.0f)), p, q),
                   exp_val);
    }

// max relative error ~3e-7
    template<class DF, class V>
    HWY_FAST_MATH_INLINE V FastPow2f(const DF df, V x) {
        const Rebind<int32_t, DF> di;
        auto floorx = Floor(x);
        auto exp =
                BitCast(df, ShiftLeft<23>(Add(ConvertTo(di, floorx), Set(di, 127))));
        auto frac = Sub(x, floorx);
        auto num = Add(frac, Set(df, 1.01749063e+01));
        num = MulAdd(num, frac, Set(df, 4.88687798e+01));
        num = MulAdd(num, frac, Set(df, 9.85506591e+01));
        num = Mul(num, exp);
        auto den = MulAdd(frac, Set(df, 2.10242958e-01), Set(df, -2.22328856e-02));
        den = MulAdd(den, frac, Set(df, -1.94414990e+01));
        den = MulAdd(den, frac, Set(df, 9.85506633e+01));
        return Div(num, den);
    }

// max relative error ~3e-5
    template<class DF, class V>
    HWY_FAST_MATH_INLINE V FastPowf(const DF df, V base, V exponent) {
        return FastPow2f(df, Mul(FastLog2f(df, base), exponent));
    }

    template<class DF, class V>
    HWY_FAST_MATH_INLINE V FastSinf(const DF df, V x) {
/*
 *     constexpr T A = T(4.0) / (T(M_PI) * T(M_PI));
    constexpr T P = 0.1952403377008734;
    constexpr T Q = 0.01915214119105392;

    T y = A * x * (T(M_PI) - x);

    return y * ((1 - P - Q) + y * (P + y * Q));
 */
        const auto A = Set(df, 4.0f / (M_PI * M_PI));
        const auto P = Set(df, 0.1952403377008734);
        const auto Q = Set(df, 0.01915214119105392);
        const auto xPi = Set(df, M_PI);
        const auto y = Mul(Mul(Sub(xPi, x), x), A);
        const auto ones = Set(df, 1.0);
        const auto mult1 = Sub(Sub(ones, P), Q);
        return MulAdd(y, mult1, Mul(y, MulAdd(y, Q, P)));
    }

// Computes cosine like std::cos.
// L1 error 7e-5.
    template<class DF, class V>
    HWY_FAST_MATH_INLINE V FastCosf(const DF df, V x) {
        // Step 1: range reduction to [0, 2pi)
        const auto pi2 = Set(df, kPi * 2.0f);
        const auto pi2_inv = Set(df, 0.5f / kPi);
        const auto npi2 = Mul(Floor(Mul(x, pi2_inv)), pi2);
        const auto xmodpi2 = Sub(x, npi2);
        // Step 2: range reduction to [0, pi]
        const auto x_pi = Min(xmodpi2, Sub(pi2, xmodpi2));
        // Step 3: range reduction to [0, pi/2]
        const auto above_pihalf = Ge(x_pi, Set(df, kPi / 2.0f));
        const auto x_pihalf = IfThenElse(above_pihalf, Sub(Set(df, kPi), x_pi), x_pi);
        // Step 4: Taylor-like approximation, scaled by 2**0.75 to make angle
        // duplication steps faster, on x/4.
        const auto xs = Mul(x_pihalf, Set(df, 0.25f));
        const auto x2 = Mul(xs, xs);
        const auto x4 = Mul(x2, x2);
        const auto cosx_prescaling =
                MulAdd(x4, Set(df, 0.06960438),
                       MulAdd(x2, Set(df, -0.84087373), Set(df, 1.68179268)));
        // Step 5: angle duplication.
        const auto cosx_scale1 =
                MulAdd(cosx_prescaling, cosx_prescaling, Set(df, -1.414213562));
        const auto cosx_scale2 = MulAdd(cosx_scale1, cosx_scale1, Set(df, -1));
        // Step 6: change sign if needed.
        const Rebind<uint32_t, DF> du;
        auto signbit = ShiftLeft<31>(BitCast(du, VecFromMask(df, above_pihalf)));
        return BitCast(df, Xor(signbit, BitCast(du, cosx_scale2)));
    }

// Computes the error function like std::erf.
// L1 error 7e-4.
    template<class DF, class V>
    HWY_FAST_MATH_INLINE V FastErff(const DF df, V x) {
        // Formula from
        // https://en.wikipedia.org/wiki/Error_function#Numerical_approximations
        // but constants have been recomputed.
        const auto xle0 = Le(x, Zero(df));
        const auto absx = Abs(x);
        // Compute 1 - 1 / ((((x * a + b) * x + c) * x + d) * x + 1)**4
        const auto denom1 =
                MulAdd(absx, Set(df, 7.77394369e-02), Set(df, 2.05260015e-04));
        const auto denom2 = MulAdd(denom1, absx, Set(df, 2.32120216e-01));
        const auto denom3 = MulAdd(denom2, absx, Set(df, 2.77820801e-01));
        const auto denom4 = MulAdd(denom3, absx, Set(df, 1.0f));
        const auto denom5 = Mul(denom4, denom4);
        const auto inv_denom5 = Div(Set(df, 1.0f), denom5);
        const auto result = NegMulAdd(inv_denom5, inv_denom5, Set(df, 1.0f));
        // Change sign if needed.
        const Rebind<uint32_t, DF> du;
        auto signbit = ShiftLeft<31>(BitCast(du, VecFromMask(df, xle0)));
        return BitCast(df, Xor(signbit, BitCast(du, result)));
    }

    HWY_FAST_MATH_INLINE float FastLog2f(float f) {
        HWY_CAPPED(float, 1) D;
        return GetLane(FastLog2f(D, Set(D, f)));
    }

    HWY_FAST_MATH_INLINE float FastPow2f(float f) {
        HWY_CAPPED(float, 1) D;
        return GetLane(FastPow2f(D, Set(D, f)));
    }

    HWY_FAST_MATH_INLINE float FastPowf(float b, float e) {
        HWY_CAPPED(float, 1) D;
        return GetLane(FastPowf(D, Set(D, b), Set(D, e)));
    }

    HWY_FAST_MATH_INLINE float FastCosf(float f) {
        HWY_CAPPED(float, 1) D;
        return GetLane(FastCosf(D, Set(D, f)));
    }

    HWY_FAST_MATH_INLINE float FastErff(float f) {
        HWY_CAPPED(float, 1) D;
        return GetLane(FastErff(D, Set(D, f)));
    }

// Returns cbrt(x) + add with 6 ulp max error.
// Modified from vectormath_exp.h, Apache 2 license.
// https://www.agner.org/optimize/vectorclass.zip
    template<class V>
    HWY_FAST_MATH_INLINE V CubeRootAndAdd(const V x, const V add) {
        const HWY_FULL(float) df;
        const HWY_FULL(int32_t) di;

        const auto kExpBias = Set(di, 0x54800000);  // cast(1.) + cast(1.) / 3
        const auto kExpMul = Set(di, 0x002AAAAA);   // shifted 1/3
        const auto k1_3 = Set(df, 1.0f / 3);
        const auto k4_3 = Set(df, 4.0f / 3);

        const auto xa = x;  // assume inputs never negative
        const auto xa_3 = Mul(k1_3, xa);

        // Multiply exponent by -1/3
        const auto m1 = BitCast(di, xa);
        // Special case for 0. 0 is represented with an exponent of 0, so the
        // "kExpBias - 1/3 * exp" below gives the wrong result. The IfThenZeroElse()
        // sets those values as 0, which prevents having NaNs in the computations
        // below.
        // TODO(eustas): use fused op
        const auto m2 = IfThenZeroElse(
                Eq(m1, Zero(di)), Sub(kExpBias, Mul((ShiftRight<23>(m1)), kExpMul)));
        auto r = BitCast(df, m2);

        // Newton-Raphson iterations
        for (int i = 0; i < 3; i++) {
            const auto r2 = Mul(r, r);
            r = NegMulAdd(xa_3, Mul(r2, r2), Mul(k4_3, r));
        }
        // Final iteration
        auto r2 = Mul(r, r);
        r = MulAdd(k1_3, NegMulAdd(xa, Mul(r2, r2), r), r);
        r2 = Mul(r, r);
        r = MulAdd(r2, x, add);

        return r;
    }

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#endif  // LIB_JXL_BASE_FAST_MATH_INL_H_

#if HWY_ONCE
#ifndef LIB_JXL_BASE_FAST_MATH_ONCE
#define LIB_JXL_BASE_FAST_MATH_ONCE

namespace coder {
    inline float FastLog2f(float f) { return HWY_STATIC_DISPATCH(FastLog2f)(f); }

    inline float FastPow2f(float f) { return HWY_STATIC_DISPATCH(FastPow2f)(f); }

    inline float FastPowf(float b, float e) {
        return HWY_STATIC_DISPATCH(FastPowf)(b, e);
    }

    inline float FastCosf(float f) { return HWY_STATIC_DISPATCH(FastCosf)(f); }

    inline float FastErff(float f) { return HWY_STATIC_DISPATCH(FastErff)(f); }
}  // namespace jxl

#endif  // LIB_JXL_BASE_FAST_MATH_ONCE
#endif  // HWY_ONCE
