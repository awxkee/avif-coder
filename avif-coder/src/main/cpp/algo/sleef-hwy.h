
// This file is translated from the SLEEF vectorized math library.
// Translation performed by Ben Parks copyright 2024.
// Translated elements available under the following licenses, at your option:
//   BSL-1.0 (http://www.boost.org/LICENSE_1_0.txt),
//   MIT (https://opensource.org/license/MIT/), and
//   Apache-2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// 
// Original SLEEF copyright:
//   Copyright Naoki Shibata and contributors 2010 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(HIGHWAY_HWY_CONTRIB_SLEEF_SLEEF_INL_) == \
    defined(HWY_TARGET_TOGGLE)  // NOLINT
#ifdef HIGHWAY_HWY_CONTRIB_SLEEF_SLEEF_INL_
#undef HIGHWAY_HWY_CONTRIB_SLEEF_SLEEF_INL_
#else
#define HIGHWAY_HWY_CONTRIB_SLEEF_SLEEF_INL_
#endif

#include "hwy/highway.h"

extern const float PayneHanekReductionTable_float[]; // Precomputed table of exponent values for Payne Hanek reduction

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace sleef {

// Computes e^x
// Translated from libm/sleefsimdsp.c:2023 xexpf
template<class D>
HWY_INLINE Vec<D> Exp(const D df, Vec<D> d);

// Computes e^x - 1
// Translated from libm/sleefsimdsp.c:2701 xexpm1f
template<class D>
HWY_INLINE Vec<D> Expm1(const D df, Vec<D> a);

// Computes ln(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:2268 xlogf_u1
template<class D>
HWY_INLINE Vec<D> Log(const D df, Vec<D> d);

// Computes ln(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:1984 xlogf
template<class D>
HWY_INLINE Vec<D> LogFast(const D df, Vec<D> d);

// Computes log1p(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:2842 xlog1pf
template<class D>
HWY_INLINE Vec<D> Log1p(const D df, Vec<D> d);

// Computes log2(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:2757 xlog2f
template<class D>
HWY_INLINE Vec<D> Log2(const D df, Vec<D> d);

// Computes sin(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:969 xsinf_u1
template<class D>
HWY_INLINE Vec<D> Sin(const D df, Vec<D> d);

// Computes cos(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:1067 xcosf_u1
template<class D>
HWY_INLINE Vec<D> Cos(const D df, Vec<D> d);

// Computes tan(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:1635 xtanf_u1
template<class D>
HWY_INLINE Vec<D> Tan(const D df, Vec<D> d);

// Computes sin(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:630 xsinf
template<class D>
HWY_INLINE Vec<D> SinFast(const D df, Vec<D> d);

// Computes cos(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:736 xcosf
template<class D>
HWY_INLINE Vec<D> CosFast(const D df, Vec<D> d);

// Computes tan(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:845 xtanf
template<class D>
HWY_INLINE Vec<D> TanFast(const D df, Vec<D> d);

// Computes sinh(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:2447 xsinhf
template<class D>
HWY_INLINE Vec<D> Sinh(const D df, Vec<D> x);

// Computes cosh(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:2461 xcoshf
template<class D>
HWY_INLINE Vec<D> Cosh(const D df, Vec<D> x);

// Computes tanh(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:2474 xtanhf
template<class D>
HWY_INLINE Vec<D> Tanh(const D df, Vec<D> x);

// Computes sinh(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:2489 xsinhf_u35
template<class D>
HWY_INLINE Vec<D> SinhFast(const D df, Vec<D> x);

// Computes cosh(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:2502 xcoshf_u35
template<class D>
HWY_INLINE Vec<D> CoshFast(const D df, Vec<D> x);

// Computes tanh(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:2513 xtanhf_u35
template<class D>
HWY_INLINE Vec<D> TanhFast(const D df, Vec<D> x);

// Computes acos(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:1948 xacosf_u1
template<class D>
HWY_INLINE Vec<D> Acos(const D df, Vec<D> d);

// Computes asin(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:1928 xasinf_u1
template<class D>
HWY_INLINE Vec<D> Asin(const D df, Vec<D> d);

// Computes asinh(x) with 1 ULP accuracy
// Translated from libm/sleefsimdsp.c:2554 xasinhf
template<class D>
HWY_INLINE Vec<D> Asinh(const D df, Vec<D> x);

// Computes acos(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:1847 xacosf
template<class D>
HWY_INLINE Vec<D> AcosFast(const D df, Vec<D> d);

// Computes asin(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:1831 xasinf
template<class D>
HWY_INLINE Vec<D> AsinFast(const D df, Vec<D> d);

// Computes atan(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:1743 xatanf
template<class D>
HWY_INLINE Vec<D> AtanFast(const D df, Vec<D> d);

// Computes acosh(x) with 1 ULP accuracy
// Translated from libm/sleefsimdsp.c:2575 xacoshf
template<class D>
HWY_INLINE Vec<D> Acosh(const D df, Vec<D> x);

// Computes atan(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:1973 xatanf_u1
template<class D>
HWY_INLINE Vec<D> Atan(const D df, Vec<D> d);

// Computes atanh(x) with 1 ULP accuracy
// Translated from libm/sleefsimdsp.c:2591 xatanhf
template<class D>
HWY_INLINE Vec<D> Atanh(const D df, Vec<D> x);

namespace {


// Estrin's Scheme is a faster method for evaluating large polynomials on
// super scalar architectures. It works by factoring the Horner's Method
// polynomial into power of two sub-trees that can be evaluated in parallel.
// Wikipedia Link: https://en.wikipedia.org/wiki/Estrin%27s_scheme
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1) {
  return MulAdd(c1, x, c0);
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T c0, T c1, T c2) {
  return MulAdd(x2, c2, MulAdd(c1, x, c0));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T c0, T c1, T c2, T c3) {
  return MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T x4, T c0, T c1, T c2, T c3, T c4) {
  return MulAdd(x4, c4, MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0)));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T x4, T c0, T c1, T c2, T c3, T c4, T c5) {
  return MulAdd(x4, MulAdd(c5, x, c4),
                MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0)));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T x4, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6) {
  return MulAdd(x4, MulAdd(x2, c6, MulAdd(c5, x, c4)),
                MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0)));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T x4, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7) {
  return MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0)));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T x4, T x8, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8) {
  return MulAdd(x8, c8,
                MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                       MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T x4, T x8, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9) {
  return MulAdd(x8, MulAdd(c9, x, c8),
                MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                       MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T x4, T x8, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10) {
  return MulAdd(x8, MulAdd(x2, c10, MulAdd(c9, x, c8)),
                MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                       MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T x4, T x8, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11) {
  return MulAdd(x8, MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8)),
                MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                       MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T x4, T x8, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11,
                                     T c12) {
  return MulAdd(
      x8, MulAdd(x4, c12, MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8))),
      MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
             MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T x4, T x8, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11,
                                     T c12, T c13) {
  return MulAdd(x8,
                MulAdd(x4, MulAdd(c13, x, c12),
                       MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8))),
                MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                       MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T x4, T x8, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11,
                                     T c12, T c13, T c14) {
  return MulAdd(x8,
                MulAdd(x4, MulAdd(x2, c14, MulAdd(c13, x, c12)),
                       MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8))),
                MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                       MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T x4, T x8, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11,
                                     T c12, T c13, T c14, T c15) {
  return MulAdd(x8,
                MulAdd(x4, MulAdd(x2, MulAdd(c15, x, c14), MulAdd(c13, x, c12)),
                       MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8))),
                MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                       MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T x4, T x8, T x16, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11,
                                     T c12, T c13, T c14, T c15, T c16) {
  return MulAdd(
      x16, c16,
      MulAdd(x8,
             MulAdd(x4, MulAdd(x2, MulAdd(c15, x, c14), MulAdd(c13, x, c12)),
                    MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8))),
             MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                    MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0)))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T x4, T x8, T x16, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11,
                                     T c12, T c13, T c14, T c15, T c16, T c17) {
  return MulAdd(
      x16, MulAdd(c17, x, c16),
      MulAdd(x8,
             MulAdd(x4, MulAdd(x2, MulAdd(c15, x, c14), MulAdd(c13, x, c12)),
                    MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8))),
             MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                    MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0)))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T x2, T x4, T x8, T x16, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11,
                                     T c12, T c13, T c14, T c15, T c16, T c17,
                                     T c18) {
  return MulAdd(
      x16, MulAdd(x2, c18, MulAdd(c17, x, c16)),
      MulAdd(x8,
             MulAdd(x4, MulAdd(x2, MulAdd(c15, x, c14), MulAdd(c13, x, c12)),
                    MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8))),
             MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                    MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0)))));
}

//////////////////
// Constants
//////////////////
constexpr double Pi = 3.141592653589793238462643383279502884; // pi
constexpr float OneOverPi = 0.318309886183790671537767526745028724; // 1 / pi
constexpr float FloatMin = 0x1p-126; // Minimum normal float value
constexpr float PiAf = 3.140625f; // Four-part sum of Pi (1/4)
constexpr float PiBf = 0.0009670257568359375f; // Four-part sum of Pi (2/4)
constexpr float PiCf = 6.2771141529083251953e-07f; // Four-part sum of Pi (3/4)
constexpr float PiDf = 1.2154201256553420762e-10f; // Four-part sum of Pi (4/4)
constexpr float TrigRangeMax = 39000; // Max value for using 4-part sum of Pi
constexpr float PiA2f = 3.1414794921875f; // Three-part sum of Pi (1/3)
constexpr float PiB2f = 0.00011315941810607910156f; // Three-part sum of Pi (2/3)
constexpr float PiC2f = 1.9841872589410058936e-09f; // Three-part sum of Pi (3/3)
constexpr float TrigRangeMax2 = 125.0f; // Max value for using 3-part sum of Pi
constexpr float SqrtFloatMax = 18446743523953729536.0; // Square root of max floating-point number
constexpr float Ln2Hi = 0.693145751953125f; // Ln2Hi + Ln2Lo ~= ln(2)
constexpr float Ln2Lo = 1.428606765330187045e-06f; // Ln2Hi + Ln2Lo ~= ln(2)
constexpr float OneOverLn2 = 1.442695040888963407359924681001892137426645954152985934135449406931f; // 1 / ln(2)
constexpr float Pif = ((float)M_PI); // pi (float)
#if (defined (__GNUC__) || defined (__clang__) || defined(__INTEL_COMPILER)) && !defined(_MSC_VER)
constexpr double NanDouble = __builtin_nan(""); // Double precision NaN
constexpr float NanFloat = __builtin_nanf(""); // Floating point NaN
constexpr double InfDouble = __builtin_inf(); // Double precision infinity
constexpr float InfFloat = __builtin_inff(); // Floating point infinity
#elif defined(_MSC_VER) 
constexpr double InfDouble = (1e+300 * 1e+300); // Double precision infinity
constexpr double NanDouble = (SLEEF_INFINITY - SLEEF_INFINITY); // Double precision NaN
constexpr float InfFloat = ((float)SLEEF_INFINITY); // Floating point infinity
constexpr float NanFloat = ((float)SLEEF_NAN); // Floating point NaN
#endif


// Computes 2^x, where x is an integer.
// Translated from libm/sleefsimdsp.c:516 vpow2i_vf_vi2
template<class D>
HWY_INLINE Vec<D> Pow2I(const D df, Vec<RebindToSigned<D>> q) {
  RebindToSigned<D> di;
  
  return BitCast(df, ShiftLeft<23>(Add(q, Set(di, 0x7f))));
}

// Sets the exponent of 'x' to 2^e. Fast, but "short reach"
// Translated from libm/sleefsimdsp.c:535 vldexp2_vf_vf_vi2
template<class D>
HWY_INLINE Vec<D> LoadExp2(const D df, Vec<D> d, Vec<RebindToSigned<D>> e) {
  return Mul(Mul(d, Pow2I(df, ShiftRight<1>(e))), Pow2I(df, Sub(e, ShiftRight<1>(e))));
}

// Computes x + y in double-float precision
// Translated from common/df.h:139 dfadd2_vf2_vf2_vf
template<class D>
HWY_INLINE Vec2<D> AddDF(const D df, Vec2<D> x, Vec<D> y) {
  Vec<D> s = Add(Get2<0>(x), y);
  Vec<D> v = Sub(s, Get2<0>(x));
  Vec<D> t = Add(Sub(Get2<0>(x), Sub(s, v)), Sub(y, v));
  return Create2(df, s, Add(t, Get2<1>(x)));
}

// Computes x + y in double-float precision
// Translated from common/df.h:158 dfadd2_vf2_vf2_vf2
template<class D>
HWY_INLINE Vec2<D> AddDF(const D df, Vec2<D> x, Vec2<D> y) {
  Vec<D> s = Add(Get2<0>(x), Get2<0>(y));
  Vec<D> v = Sub(s, Get2<0>(x));
  Vec<D> t = Add(Sub(Get2<0>(x), Sub(s, v)), Sub(Get2<0>(y), v));
  return Create2(df, s, Add(t, Add(Get2<1>(x), Get2<1>(y))));
}

// Add (x + y) + z
// Translated from common/df.h:59 vadd_vf_3vf
template<class D>
HWY_INLINE Vec<D> Add3(const D df, Vec<D> v0, Vec<D> v1, Vec<D> v2) {
  return Add(Add(v0, v1), v2);
}

// Computes x + y in double-float precision, sped up by assuming |x| > |y|
// Translated from common/df.h:146 dfadd_vf2_vf_vf2
template<class D>
HWY_INLINE Vec2<D> AddFastDF(const D df, Vec<D> x, Vec2<D> y) {
  Vec<D> s = Add(x, Get2<0>(y));
  return Create2(df, s, Add3(df, Sub(x, s), Get2<0>(y), Get2<1>(y)));
}

// Set the bottom 12 significand bits of a floating point number to 0 (used in some double-float math)
// Translated from common/df.h:22 vupper_vf_vf
template<class D>
HWY_INLINE Vec<D> LowerPrecision(const D df, Vec<D> d) {
  RebindToSigned<D> di;
  
  return BitCast(df, And(BitCast(di, d), Set(di, 0xfffff000)));
}

// Computes x * y in double-float precision
// Translated from common/df.h:214 dfmul_vf2_vf2_vf
template<class D>
HWY_INLINE Vec2<D> MulDF(const D df, Vec2<D> x, Vec<D> y) {
  Vec<D> s = Mul(Get2<0>(x), y);
  return Create2(df, s, MulAdd(Get2<1>(x), y, MulSub(Get2<0>(x), y, s)));
}

// Computes x * y in double-float precision
// Translated from common/df.h:205 dfmul_vf2_vf2_vf2
template<class D>
HWY_INLINE Vec2<D> MulDF(const D df, Vec2<D> x, Vec2<D> y) {
  Vec<D> s = Mul(Get2<0>(x), Get2<0>(y));
  return Create2(df, s, MulAdd(Get2<0>(x), Get2<1>(y), MulAdd(Get2<1>(x), Get2<0>(y), MulSub(Get2<0>(x), Get2<0>(y), s))));
}

// Computes x^2 in double-float precision
// Translated from common/df.h:196 dfsqu_vf2_vf2
template<class D>
HWY_INLINE Vec2<D> SquareDF(const D df, Vec2<D> x) {
  Vec<D> s = Mul(Get2<0>(x), Get2<0>(x));
  return Create2(df, s, MulAdd(Add(Get2<0>(x), Get2<0>(x)), Get2<1>(x), MulSub(Get2<0>(x), Get2<0>(x), s)));
}

// Computes e^x in double-float precision
// Translated from libm/sleefsimdsp.c:2418 expk2f
template<class D>
HWY_INLINE Vec2<D> ExpDF(const D df, Vec2<D> d) {
  RebindToUnsigned<D> du;
  
  Vec<D> u = Mul(Add(Get2<0>(d), Get2<1>(d)), Set(df, OneOverLn2));
  Vec<RebindToSigned<D>> q = NearestInt(u);
  Vec2<D> s, t;

  s = AddDF(df, d, Mul(ConvertTo(df, q), Set(df, -Ln2Hi)));
  s = AddDF(df, s, Mul(ConvertTo(df, q), Set(df, -Ln2Lo)));

  u = Set(df, +0.1980960224e-3f);
  u = MulAdd(u, Get2<0>(s), Set(df, +0.1394256484e-2f));
  u = MulAdd(u, Get2<0>(s), Set(df, +0.8333456703e-2f));
  u = MulAdd(u, Get2<0>(s), Set(df, +0.4166637361e-1f));

  t = AddDF(df, MulDF(df, s, u), Set(df, +0.166666659414234244790680580464e+0f));
  t = AddDF(df, MulDF(df, s, t), Set(df, 0.5));
  t = AddDF(df, s, MulDF(df, SquareDF(df, s), t));

  t = AddFastDF(df, Set(df, 1), t);

  t = Set2<0>(t, LoadExp2(df, Get2<0>(t), q));
  t = Set2<1>(t, LoadExp2(df, Get2<1>(t), q));

  t = Set2<0>(t, BitCast(df, IfThenZeroElse(RebindMask(du, Lt(Get2<0>(d), Set(df, -104))), BitCast(du, Get2<0>(t)))));
  t = Set2<1>(t, BitCast(df, IfThenZeroElse(RebindMask(du, Lt(Get2<0>(d), Set(df, -104))), BitCast(du, Get2<1>(t)))));

  return t;
}

// Computes x + y in double-float precision
// Translated from common/df.h:116 dfadd2_vf2_vf_vf
template<class D>
HWY_INLINE Vec2<D> AddDF(const D df, Vec<D> x, Vec<D> y) {
  Vec<D> s = Add(x, y);
  Vec<D> v = Sub(s, x);
  return Create2(df, s, Add(Sub(x, Sub(s, v)), Sub(y, v)));
}

// Sets the exponent of 'x' to 2^e. Very fast, "no denormal"
// Translated from libm/sleefsimdsp.c:539 vldexp3_vf_vf_vi2
template<class D>
HWY_INLINE Vec<D> LoadExp3(const D df, Vec<D> d, Vec<RebindToSigned<D>> q) {
  RebindToSigned<D> di;
  
  return BitCast(df, Add(BitCast(di, d), ShiftLeft<23>(q)));
}

// Computes x * y in double-float precision
// Translated from common/df.h:107 dfscale_vf2_vf2_vf
template<class D>
HWY_INLINE Vec2<D> ScaleDF(const D df, Vec2<D> d, Vec<D> s) {
  return Create2(df, Mul(Get2<0>(d), s), Mul(Get2<1>(d), s));
}

// Computes x + y in double-float precision, sped up by assuming |x| > |y|
// Translated from common/df.h:129 dfadd_vf2_vf2_vf
template<class D>
HWY_INLINE Vec2<D> AddFastDF(const D df, Vec2<D> x, Vec<D> y) {
  Vec<D> s = Add(Get2<0>(x), y);
  return Create2(df, s, Add3(df, Sub(Get2<0>(x), s), y, Get2<1>(x)));
}

// Integer log of x, "but the argument must be a normalized value"
// Translated from libm/sleefsimdsp.c:497 vilogb2k_vi2_vf
template<class D>
HWY_INLINE Vec<RebindToSigned<D>> ILogB2(const D df, Vec<D> d) {
  RebindToUnsigned<D> du;
  RebindToSigned<D> di;
  
  Vec<RebindToSigned<D>> q = BitCast(di, d);
  q = BitCast(di, ShiftRight<23>(BitCast(du, q)));
  q = And(q, Set(di, 0xff));
  q = Sub(q, Set(di, 0x7f));
  return q;
}

// Add ((x + y) + z) + w
// Translated from common/df.h:63 vadd_vf_4vf
template<class D>
HWY_INLINE Vec<D> Add4(const D df, Vec<D> v0, Vec<D> v1, Vec<D> v2, Vec<D> v3) {
  return Add3(df, Add(v0, v1), v2, v3);
}

// Computes x + y in double-float precision, sped up by assuming |x| > |y|
// Translated from common/df.h:151 dfadd_vf2_vf2_vf2
template<class D>
HWY_INLINE Vec2<D> AddFastDF(const D df, Vec2<D> x, Vec2<D> y) {
  // |x| >= |y|

  Vec<D> s = Add(Get2<0>(x), Get2<0>(y));
  return Create2(df, s, Add4(df, Sub(Get2<0>(x), s), Get2<0>(y), Get2<1>(x), Get2<1>(y)));
}

// Computes x / y in double-float precision
// Translated from common/df.h:183 dfdiv_vf2_vf2_vf2
template<class D>
HWY_INLINE Vec2<D> DivDF(const D df, Vec2<D> n, Vec2<D> d) {
  Vec<D> t = Div(Set(df, 1.0), Get2<0>(d));
  Vec<D> s = Mul(Get2<0>(n), t);
  Vec<D> u = MulSub(t, Get2<0>(n), s);
  Vec<D> v = NegMulAdd(Get2<1>(d), t, NegMulAdd(Get2<0>(d), t, Set(df, 1)));
  return Create2(df, s, MulAdd(s, v, MulAdd(Get2<1>(n), t, u)));
}

// Computes x + y in double-float precision, sped up by assuming |x| > |y|
// Translated from common/df.h:111 dfadd_vf2_vf_vf
template<class D>
HWY_INLINE Vec2<D> AddFastDF(const D df, Vec<D> x, Vec<D> y) {
  Vec<D> s = Add(x, y);
  return Create2(df, s, Add(Sub(x, s), y));
}

// Computes x + y in double-float precision
// Translated from common/df.h:122 dfadd2_vf2_vf_vf2
template<class D>
HWY_INLINE Vec2<D> AddDF(const D df, Vec<D> x, Vec2<D> y) {
  Vec<D> s = Add(x, Get2<0>(y));
  Vec<D> v = Sub(s, x);
  return Create2(df, s, Add(Add(Sub(x, Sub(s, v)), Sub(Get2<0>(y), v)), Get2<1>(y)));

}

// Computes x * y in double-float precision, returning result as single-precision
// Translated from common/df.h:210 dfmul_vf_vf2_vf2
template<class D>
HWY_INLINE Vec<D> MulDF_float(const D df, Vec2<D> x, Vec2<D> y) {
  return MulAdd(Get2<0>(x), Get2<0>(y), MulAdd(Get2<1>(x), Get2<0>(y), Mul(Get2<0>(x), Get2<1>(y))));
}

// Extract the sign bit of x into an unsigned integer
// Translated from libm/sleefsimdsp.c:453 vsignbit_vm_vf
template<class D>
HWY_INLINE Vec<RebindToUnsigned<D>> SignBit(const D df, Vec<D> f) {
  RebindToUnsigned<D> du;
  
  return And(BitCast(du, f), BitCast(du, Set(df, -0.0f)));
}

// Calculate x * sign(y) with only bitwise logic
// Translated from libm/sleefsimdsp.c:458 vmulsign_vf_vf_vf
template<class D>
HWY_INLINE Vec<D> MulSignBit(const D df, Vec<D> x, Vec<D> y) {
  RebindToUnsigned<D> du;
  
  return BitCast(df, Xor(BitCast(du, x), SignBit(df, y)));
}

// Normalizes a double-float precision representation (redistributes hi vs. lo value)
// Translated from common/df.h:102 dfnormalize_vf2_vf2
template<class D>
HWY_INLINE Vec2<D> NormalizeDF(const D df, Vec2<D> t) {
  Vec<D> s = Add(Get2<0>(t), Get2<1>(t));
  return Create2(df, s, Add(Sub(Get2<0>(t), s), Get2<1>(t)));
}

// Specialization of IfThenElse to double-float operands
// Translated from common/df.h:38 vsel_vf2_vo_vf2_vf2
template<class D>
HWY_INLINE Vec2<D> IfThenElse(const D df, Mask<D> m, Vec2<D> x, Vec2<D> y) {
  return Create2(df, IfThenElse(RebindMask(df, m), Get2<0>(x), Get2<0>(y)), IfThenElse(RebindMask(df, m), Get2<1>(x), Get2<1>(y)));
}

// Computes x * y in double-float precision
// Translated from common/df.h:191 dfmul_vf2_vf_vf
template<class D>
HWY_INLINE Vec2<D> MulDF(const D df, Vec<D> x, Vec<D> y) {
  Vec<D> s = Mul(x, y);
  return Create2(df, s, MulSub(x, y, s));
}

// Bitwise or of x with sign bit of y
// Translated from libm/sleefsimdsp.c:576 vorsign_vf_vf_vf
template<class D>
HWY_INLINE Vec<D> OrSignBit(const D df, Vec<D> x, Vec<D> y) {
  RebindToUnsigned<D> du;
  
  return BitCast(df, Or(BitCast(du, x), SignBit(df, y)));
}

// Helper for Payne Hanek reduction.
// Translated from libm/sleefsimdsp.c:581 rempisubf
template<class D>
HWY_INLINE Vec2<D> PayneHanekReductionHelper(const D df, Vec<D> x) {
  RebindToSigned<D> di;
  
Vec<D> y = Round(Mul(x, Set(df, 4)));
  Vec<RebindToSigned<D>> vi = ConvertTo(di, Sub(y, Mul(Round(x), Set(df, 4))));
  return Create2(df, Sub(x, Mul(y, Set(df, 0.25))), BitCast(df, vi));

}

// Calculate Payne Hanek reduction. This appears to return ((2*x/pi) - round(2*x/pi)) * pi / 2 and the integer quadrant of x in range -2 to 2 (0 is [-pi/4, pi/4], 2/-2 are from [3pi/4, 5pi/4] with the sign flip a little after pi).
// Translated from libm/sleefsimdsp.c:598 rempif
template<class D>
HWY_INLINE Vec3<D> PayneHanekReduction(const D df, Vec<D> a) {
  RebindToSigned<D> di;
  
  Vec2<D> x, y;
  Vec<RebindToSigned<D>> ex = ILogB2(df, a);
#if HWY_ARCH_X86 && HWY_TARGET <= HWY_AVX3
  ex = AndNot(ShiftRight<31>(ex), ex);
  ex = And(ex, Set(di, 127));
#endif
  ex = Sub(ex, Set(di, 25));
  Vec<RebindToSigned<D>> q = IfThenElseZero(RebindMask(di, Gt(ex, Set(di, 90-25))), Set(di, -64));
  a = LoadExp3(df, a, q);
  ex = AndNot(ShiftRight<31>(ex), ex);
  ex = ShiftLeft<2>(ex);
  x = MulDF(df, a, GatherIndex(df, PayneHanekReductionTable_float, ex));
  Vec2<D> di_ = PayneHanekReductionHelper(df, Get2<0>(x));
  q = BitCast(di, Get2<1>(di_));
  x = Set2<0>(x, Get2<0>(di_));
  x = NormalizeDF(df, x);
  y = MulDF(df, a, GatherIndex(df, PayneHanekReductionTable_float+1, ex));
  x = AddDF(df, x, y);
  di_ = PayneHanekReductionHelper(df, Get2<0>(x));
  q = Add(q, BitCast(di, Get2<1>(di_)));
  x = Set2<0>(x, Get2<0>(di_));
  x = NormalizeDF(df, x);
  y = Create2(df, GatherIndex(df, PayneHanekReductionTable_float+2, ex), GatherIndex(df, PayneHanekReductionTable_float+3, ex));
  y = MulDF(df, y, a);
  x = AddDF(df, x, y);
  x = NormalizeDF(df, x);
  x = MulDF(df, x, Create2(df, Set(df, 3.1415927410125732422f*2), Set(df, -8.7422776573475857731e-08f*2)));
  x = IfThenElse(df, Lt(Abs(a), Set(df, 0.7f)), Create2(df, a, Set(df, 0)), x);
  return Create3(df, Get2<0>(x), Get2<1>(x), BitCast(df, q));
}

// Computes 1/x in double-float precision
// Translated from common/df.h:224 dfrec_vf2_vf2
template<class D>
HWY_INLINE Vec2<D> RecDF(const D df, Vec2<D> d) {
  Vec<D> s = Div(Set(df, 1.0), Get2<0>(d));
  return Create2(df, s, Mul(s, NegMulAdd(Get2<1>(d), s, NegMulAdd(Get2<0>(d), s, Set(df, 1)))));
}

// Computes x - y in double-float precision, assuming |x| > |y|
// Translated from common/df.h:172 dfsub_vf2_vf2_vf2
template<class D>
HWY_INLINE Vec2<D> SubDF(const D df, Vec2<D> x, Vec2<D> y) {
  // |x| >= |y|

  Vec<D> s = Sub(Get2<0>(x), Get2<0>(y));
  Vec<D> t = Sub(Get2<0>(x), s);
  t = Sub(t, Get2<0>(y));
  t = Add(t, Get2<1>(x));
  return Create2(df, s, Sub(t, Get2<1>(y)));
}

// Computes -x in double-float precision
// Translated from common/df.h:93 dfneg_vf2_vf2
template<class D>
HWY_INLINE Vec2<D> NegDF(const D df, Vec2<D> x) {
  return Create2(df, Neg(Get2<0>(x)), Neg(Get2<1>(x)));
}

// Computes e^x - 1 faster with lower precision
// Translated from libm/sleefsimdsp.c:2048 expm1fk
template<class D>
HWY_INLINE Vec<D> Expm1Fast(const D df, Vec<D> d) {
  RebindToSigned<D> di;
  
  Vec<RebindToSigned<D>> q = NearestInt(Mul(d, Set(df, OneOverLn2)));
  Vec<D> s, u;

  s = MulAdd(ConvertTo(df, q), Set(df, -Ln2Hi), d);
  s = MulAdd(ConvertTo(df, q), Set(df, -Ln2Lo), s);

  Vec<D> s2 = Mul(s, s), s4 = Mul(s2, s2);
  u = Estrin(s, s2, s4, Set(df, 0.5), Set(df, 0.166666671633720397949219), Set(df, 0.0416664853692054748535156), Set(df, 0.00833336077630519866943359), Set(df, 0.00139304355252534151077271), Set(df, 0.000198527617612853646278381));

  u = MulAdd(Mul(s, s), u, s);

  u = IfThenElse(RebindMask(df, RebindMask(df, Eq(q, Set(di, 0)))), u, Sub(LoadExp2(df, Add(u, Set(df, 1)), q), Set(df, 1)));

  return u;
}

// Computes 1/x in double-float precision
// Translated from common/df.h:219 dfrec_vf2_vf
template<class D>
HWY_INLINE Vec2<D> RecDF(const D df, Vec<D> d) {
  Vec<D> s = Div(Set(df, 1.0), d);
  return Create2(df, s, Mul(s, NegMulAdd(d, s, Set(df, 1))));
}

// Computes sqrt(x) in double-float precision
// Translated from common/df.h:366 dfsqrt_vf2_vf
template<class D>
HWY_INLINE Vec2<D> SqrtDF(const D df, Vec<D> d) {
  Vec<D> t = Sqrt(d);
  return ScaleDF(df, MulDF(df, AddDF(df, d, MulDF(df, t, t)), RecDF(df, t)), Set(df, 0.5f));
}

// Computes x - y in double-float precision, assuming |x| > |y|
// Translated from common/df.h:134 dfsub_vf2_vf2_vf
template<class D>
HWY_INLINE Vec2<D> SubDF(const D df, Vec2<D> x, Vec<D> y) {
  Vec<D> s = Sub(Get2<0>(x), y);
  return Create2(df, s, Add(Sub(Sub(Get2<0>(x), s), y), Get2<1>(x)));
}

// Computes sqrt(x) in double-float precision
// Translated from common/df.h:355 dfsqrt_vf2_vf2
template<class D>
HWY_INLINE Vec2<D> SqrtDF(const D df, Vec2<D> d) {
  Vec<D> t = Sqrt(Add(Get2<0>(d), Get2<1>(d)));
  return ScaleDF(df, MulDF(df, AddDF(df, d, MulDF(df, t, t)), RecDF(df, t)), Set(df, 0.5));
}

// Integer log of x
// Translated from libm/sleefsimdsp.c:489 vilogbk_vi2_vf
template<class D>
HWY_INLINE Vec<RebindToSigned<D>> ILogB(const D df, Vec<D> d) {
  RebindToUnsigned<D> du;
  RebindToSigned<D> di;
  
  Mask<D> o = Lt(d, Set(df, 5.421010862427522E-20f));
  d = IfThenElse(RebindMask(df, o), Mul(Set(df, 1.8446744073709552E19f), d), d);
  Vec<RebindToSigned<D>> q = And(BitCast(di, ShiftRight<23>(BitCast(du, BitCast(di, d)))), Set(di, 0xff));
  q = Sub(q, IfThenElse(RebindMask(di, o), Set(di, 64 + 0x7f), Set(di, 0x7f)));
  return q;
}

// Computes ln(x) in double-float precision (version 2)
// Translated from libm/sleefsimdsp.c:2526 logk2f
template<class D>
HWY_INLINE Vec2<D> LogFastDF(const D df, Vec2<D> d) {
  Vec2<D> x, x2, m, s;
  Vec<D> t;
  Vec<RebindToSigned<D>> e;

#if !(HWY_ARCH_X86 && HWY_TARGET <= HWY_AVX3)
  e = ILogB(df, Mul(Get2<0>(d), Set(df, 1.0f/0.75f)));
#else
  e = NearestInt(_mm512_getexp_ps(f.raw));
#endif
  m = ScaleDF(df, d, Pow2I(df, Neg(e)));

  x = DivDF(df, AddDF(df, m, Set(df, -1)), AddDF(df, m, Set(df, 1)));
  x2 = SquareDF(df, x);

  t = Set(df, 0.2392828464508056640625f);
  t = MulAdd(t, Get2<0>(x2), Set(df, 0.28518211841583251953125f));
  t = MulAdd(t, Get2<0>(x2), Set(df, 0.400005877017974853515625f));
  t = MulAdd(t, Get2<0>(x2), Set(df, 0.666666686534881591796875f));

  s = MulDF(df, Create2(df, Set(df, 0.69314718246459960938f), Set(df, -1.904654323148236017e-09f)), ConvertTo(df, e));
  s = AddFastDF(df, s, ScaleDF(df, x, Set(df, 2)));
  s = AddFastDF(df, s, MulDF(df, MulDF(df, x2, x), t));

  return s;
}

// Create a mask of which is true if x's sign bit is set
// Translated from libm/sleefsimdsp.c:472 vsignbit_vo_vf
template<class D>
HWY_INLINE Mask<D> SignBitMask(const D df, Vec<D> d) {
  RebindToSigned<D> di;
  
  return RebindMask(df, Eq(And(BitCast(di, d), Set(di, 0x80000000)), Set(di, 0x80000000)));
}

template<class DF, class V>
HWY_INLINE V
Log10f(const DF df, V x) {
    const auto baseScalar = Set(df, 10.0f);
    const auto base = LogFast(df, baseScalar);
    const auto logNum = LogFast(df, x);
    return Div(logNum, base);
}

// Zero out x when the sign bit of d is not set
// Translated from libm/sleefsimdsp.c:480 vsel_vi2_vf_vi2
template<class D>
HWY_INLINE Vec<RebindToSigned<D>> SignBitOrZero(const D df, Vec<D> d, Vec<RebindToSigned<D>> x) {
  RebindToSigned<D> di;

  return IfThenElseZero(RebindMask(di, SignBitMask(df, d)), x);
}

// atan2(x, y) in double-float precision
// Translated from libm/sleefsimdsp.c:1872 atan2kf_u1
template<class D>
HWY_INLINE Vec2<D> ATan2DF(const D df, Vec2<D> y, Vec2<D> x) {
  RebindToUnsigned<D> du;
  RebindToSigned<D> di;
  
  Vec<D> u;
  Vec2<D> s, t;
  Vec<RebindToSigned<D>> q;
  Mask<D> p;
  Vec<RebindToUnsigned<D>> r;
  
  q = IfThenElse(RebindMask(di, Lt(Get2<0>(x), Set(df, 0))), Set(di, -2), Set(di, 0));
  p = Lt(Get2<0>(x), Set(df, 0));
  r = IfThenElseZero(RebindMask(du, p), BitCast(du, Set(df, -0.0)));
  x = Set2<0>(x, BitCast(df, Xor(BitCast(du, Get2<0>(x)), r)));
  x = Set2<1>(x, BitCast(df, Xor(BitCast(du, Get2<1>(x)), r)));

  q = IfThenElse(RebindMask(di, Lt(Get2<0>(x), Get2<0>(y))), Add(q, Set(di, 1)), q);
  p = Lt(Get2<0>(x), Get2<0>(y));
  s = IfThenElse(df, p, NegDF(df, x), y);
  t = IfThenElse(df, p, y, x);

  s = DivDF(df, s, t);
  t = SquareDF(df, s);
  t = NormalizeDF(df, t);

  u = Set(df, -0.00176397908944636583328247f);
  u = MulAdd(u, Get2<0>(t), Set(df, 0.0107900900766253471374512f));
  u = MulAdd(u, Get2<0>(t), Set(df, -0.0309564601629972457885742f));
  u = MulAdd(u, Get2<0>(t), Set(df, 0.0577365085482597351074219f));
  u = MulAdd(u, Get2<0>(t), Set(df, -0.0838950723409652709960938f));
  u = MulAdd(u, Get2<0>(t), Set(df, 0.109463557600975036621094f));
  u = MulAdd(u, Get2<0>(t), Set(df, -0.142626821994781494140625f));
  u = MulAdd(u, Get2<0>(t), Set(df, 0.199983194470405578613281f));

  t = MulDF(df, t, AddFastDF(df, Set(df, -0.333332866430282592773438f), Mul(u, Get2<0>(t))));
  t = MulDF(df, s, AddFastDF(df, Set(df, 1), t));
  t = AddFastDF(df, MulDF(df, Create2(df, Set(df, 1.5707963705062866211f), Set(df, -4.3711388286737928865e-08f)), ConvertTo(df, q)), t);

  return t;
}

}

// Computes e^x
// Translated from libm/sleefsimdsp.c:2023 xexpf
template<class D>
HWY_INLINE Vec<D> Exp(const D df, Vec<D> d) {
  RebindToUnsigned<D> du;
  
  Vec<RebindToSigned<D>> q = NearestInt(Mul(d, Set(df, OneOverLn2)));
  Vec<D> s, u;

  s = MulAdd(ConvertTo(df, q), Set(df, -Ln2Hi), d);
  s = MulAdd(ConvertTo(df, q), Set(df, -Ln2Lo), s);

  u = Set(df, 0.000198527617612853646278381);
  u = MulAdd(u, s, Set(df, 0.00139304355252534151077271));
  u = MulAdd(u, s, Set(df, 0.00833336077630519866943359));
  u = MulAdd(u, s, Set(df, 0.0416664853692054748535156));
  u = MulAdd(u, s, Set(df, 0.166666671633720397949219));
  u = MulAdd(u, s, Set(df, 0.5));

  u = Add(Set(df, 1.0f), MulAdd(Mul(s, s), u, s));

  u = LoadExp2(df, u, q);

  u = BitCast(df, IfThenZeroElse(RebindMask(du, Lt(d, Set(df, -104))), BitCast(du, u)));
  u = IfThenElse(RebindMask(df, Lt(Set(df, 100), d)), Set(df, InfFloat), u);

  return u;
}

// Computes e^x - 1
// Translated from libm/sleefsimdsp.c:2701 xexpm1f
template<class D>
HWY_INLINE Vec<D> Expm1(const D df, Vec<D> a) {
  RebindToUnsigned<D> du;
  
  Vec2<D> d = AddDF(df, ExpDF(df, Create2(df, a, Set(df, 0))), Set(df, -1.0));
  Vec<D> x = Add(Get2<0>(d), Get2<1>(d));
  x = IfThenElse(RebindMask(df, Gt(a, Set(df, 88.72283172607421875f))), Set(df, InfFloat), x);
  x = IfThenElse(RebindMask(df, Lt(a, Set(df, -16.635532333438687426013570f))), Set(df, -1), x);
  x = IfThenElse(RebindMask(df, Eq(BitCast(du, a), Set(du, 0x80000000))), Set(df, -0.0f), x);
  return x;
}

// Computes ln(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:2268 xlogf_u1
template<class D>
HWY_INLINE Vec<D> Log(const D df, Vec<D> d) {
  RebindToSigned<D> di;
  
  Vec2<D> x;
  Vec<D> t, m, x2;

#if !(HWY_ARCH_X86 && HWY_TARGET <= HWY_AVX3)
  Mask<D> o = Lt(d, Set(df, FloatMin));
  d = IfThenElse(RebindMask(df, o), Mul(d, Set(df, (float)(INT64_C(1) << 32) * (float)(INT64_C(1) << 32))), d);
  Vec<RebindToSigned<D>> e = ILogB2(df, Mul(d, Set(df, 1.0f/0.75f)));
  m = LoadExp3(df, d, Neg(e));
  e = IfThenElse(RebindMask(di, o), Sub(e, Set(di, 64)), e);
  Vec2<D> s = MulDF(df, Create2(df, Set(df, 0.69314718246459960938f), Set(df, -1.904654323148236017e-09f)), ConvertTo(df, e));
#else
  Vec<D> e = _mm512_getexp_ps(f.raw);
  e = IfThenElse(RebindMask(df, Eq(e, Inf(df))), Set(df, 128.0f), e);
  m = _mm512_getmant_ps(f.raw, _MM_MANT_NORM_p75_1p5, _MM_MANT_SIGN_nan);
  Vec2<D> s = MulDF(df, Create2(df, Set(df, 0.69314718246459960938f), Set(df, -1.904654323148236017e-09f)), e);
#endif

  x = DivDF(df, AddDF(df, Set(df, -1), m), AddDF(df, Set(df, 1), m));
  x2 = Mul(Get2<0>(x), Get2<0>(x));

  t = Set(df, +0.3027294874e+0f);
  t = MulAdd(t, x2, Set(df, +0.3996108174e+0f));
  t = MulAdd(t, x2, Set(df, +0.6666694880e+0f));
  
  s = AddFastDF(df, s, ScaleDF(df, x, Set(df, 2)));
  s = AddFastDF(df, s, Mul(Mul(x2, Get2<0>(x)), t));

  Vec<D> r = Add(Get2<0>(s), Get2<1>(s));

#if !(HWY_ARCH_X86 && HWY_TARGET <= HWY_AVX3)
  r = IfThenElse(RebindMask(df, Eq(d, Inf(df))), Set(df, InfFloat), r);
  r = IfThenElse(RebindMask(df, Or(Lt(d, Set(df, 0)), IsNaN(d))), Set(df, NanFloat), r);
  r = IfThenElse(RebindMask(df, Eq(d, Set(df, 0))), Set(df, -InfFloat), r);
#else
  r = vfixup_vf_vf_vf_vi2_i(r, d, Set(di, (4 << (2*4)) | (3 << (4*4)) | (5 << (5*4)) | (2 << (6*4))), 0);
#endif
  
  return r;
}

// Computes ln(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:1984 xlogf
template<class D>
HWY_INLINE Vec<D> LogFast(const D df, Vec<D> d) {
  RebindToSigned<D> di;
  
  Vec<D> x, x2, t, m;

#if !(HWY_ARCH_X86 && HWY_TARGET <= HWY_AVX3)
  Mask<D> o = Lt(d, Set(df, FloatMin));
  d = IfThenElse(RebindMask(df, o), Mul(d, Set(df, (float)(INT64_C(1) << 32) * (float)(INT64_C(1) << 32))), d);
  Vec<RebindToSigned<D>> e = ILogB2(df, Mul(d, Set(df, 1.0f/0.75f)));
  m = LoadExp3(df, d, Neg(e));
  e = IfThenElse(RebindMask(di, o), Sub(e, Set(di, 64)), e);
#else
  Vec<D> e = _mm512_getexp_ps(f.raw);
  e = IfThenElse(RebindMask(df, Eq(e, Inf(df))), Set(df, 128.0f), e);
  m = _mm512_getmant_ps(f.raw, _MM_MANT_NORM_p75_1p5, _MM_MANT_SIGN_nan);
#endif
  
  x = Div(Sub(m, Set(df, 1.0f)), Add(Set(df, 1.0f), m));
  x2 = Mul(x, x);

  t = Set(df, 0.2392828464508056640625f);
  t = MulAdd(t, x2, Set(df, 0.28518211841583251953125f));
  t = MulAdd(t, x2, Set(df, 0.400005877017974853515625f));
  t = MulAdd(t, x2, Set(df, 0.666666686534881591796875f));
  t = MulAdd(t, x2, Set(df, 2.0f));

#if !(HWY_ARCH_X86 && HWY_TARGET <= HWY_AVX3)
  x = MulAdd(x, t, Mul(Set(df, 0.693147180559945286226764f), ConvertTo(df, e)));
  x = IfThenElse(RebindMask(df, Eq(d, Inf(df))), Set(df, InfFloat), x);
  x = IfThenElse(RebindMask(df, Or(Lt(d, Set(df, 0)), IsNaN(d))), Set(df, NanFloat), x);
  x = IfThenElse(RebindMask(df, Eq(d, Set(df, 0))), Set(df, -InfFloat), x);
#else
  x = MulAdd(x, t, Mul(Set(df, 0.693147180559945286226764f), e));
  x = vfixup_vf_vf_vf_vi2_i(x, d, Set(di, (5 << (5*4))), 0);
#endif
  
  return x;
}

// Computes log1p(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:2842 xlog1pf
template<class D>
HWY_INLINE Vec<D> Log1p(const D df, Vec<D> d) {
  RebindToUnsigned<D> du;
  RebindToSigned<D> di;
  
  Vec2<D> x;
  Vec<D> t, m, x2;

  Vec<D> dp1 = Add(d, Set(df, 1));

#if !(HWY_ARCH_X86 && HWY_TARGET <= HWY_AVX3)
  Mask<D> o = Lt(dp1, Set(df, FloatMin));
  dp1 = IfThenElse(RebindMask(df, o), Mul(dp1, Set(df, (float)(INT64_C(1) << 32) * (float)(INT64_C(1) << 32))), dp1);
  Vec<RebindToSigned<D>> e = ILogB2(df, Mul(dp1, Set(df, 1.0f/0.75f)));
  t = LoadExp3(df, Set(df, 1), Neg(e));
  m = MulAdd(d, t, Sub(t, Set(df, 1)));
  e = IfThenElse(RebindMask(di, o), Sub(e, Set(di, 64)), e);
  Vec2<D> s = MulDF(df, Create2(df, Set(df, 0.69314718246459960938f), Set(df, -1.904654323148236017e-09f)), ConvertTo(df, e));
#else
  Vec<D> e = _mm512_getexp_ps(f.raw);
  e = IfThenElse(RebindMask(df, Eq(e, Inf(df))), Set(df, 128.0f), e);
  t = LoadExp3(df, Set(df, 1), Neg(NearestInt(e)));
  m = MulAdd(d, t, Sub(t, Set(df, 1)));
  Vec2<D> s = MulDF(df, Create2(df, Set(df, 0.69314718246459960938f), Set(df, -1.904654323148236017e-09f)), e);
#endif

  x = DivDF(df, Create2(df, m, Set(df, 0)), AddFastDF(df, Set(df, 2), m));
  x2 = Mul(Get2<0>(x), Get2<0>(x));

  t = Set(df, +0.3027294874e+0f);
  t = MulAdd(t, x2, Set(df, +0.3996108174e+0f));
  t = MulAdd(t, x2, Set(df, +0.6666694880e+0f));
  
  s = AddFastDF(df, s, ScaleDF(df, x, Set(df, 2)));
  s = AddFastDF(df, s, Mul(Mul(x2, Get2<0>(x)), t));

  Vec<D> r = Add(Get2<0>(s), Get2<1>(s));
  
  r = IfThenElse(RebindMask(df, Gt(d, Set(df, 1e+38))), Set(df, InfFloat), r);
  r = BitCast(df, IfThenElse(RebindMask(du, Gt(Set(df, -1), d)), Set(du, -1), BitCast(du, r)));
  r = IfThenElse(RebindMask(df, Eq(d, Set(df, -1))), Set(df, -InfFloat), r);
  r = IfThenElse(RebindMask(df, Eq(BitCast(du, d), Set(du, 0x80000000))), Set(df, -0.0f), r);

  return r;
}

// Computes log2(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:2757 xlog2f
template<class D>
HWY_INLINE Vec<D> Log2(const D df, Vec<D> d) {
  RebindToSigned<D> di;
  
  Vec2<D> x;
  Vec<D> t, m, x2;

#if !(HWY_ARCH_X86 && HWY_TARGET <= HWY_AVX3)
  Mask<D> o = Lt(d, Set(df, FloatMin));
  d = IfThenElse(RebindMask(df, o), Mul(d, Set(df, (float)(INT64_C(1) << 32) * (float)(INT64_C(1) << 32))), d);
  Vec<RebindToSigned<D>> e = ILogB2(df, Mul(d, Set(df, 1.0/0.75)));
  m = LoadExp3(df, d, Neg(e));
  e = IfThenElse(RebindMask(di, o), Sub(e, Set(di, 64)), e);
#else
  Vec<D> e = _mm512_getexp_ps(f.raw);
  e = IfThenElse(RebindMask(df, Eq(e, Inf(df))), Set(df, 128.0f), e);
  m = _mm512_getmant_ps(f.raw, _MM_MANT_NORM_p75_1p5, _MM_MANT_SIGN_nan);
#endif

  x = DivDF(df, AddDF(df, Set(df, -1), m), AddDF(df, Set(df, 1), m));
  x2 = Mul(Get2<0>(x), Get2<0>(x));

  t = Set(df, +0.4374550283e+0f);
  t = MulAdd(t, x2, Set(df, +0.5764790177e+0f));
  t = MulAdd(t, x2, Set(df, +0.9618012905120f));
  
#if !(HWY_ARCH_X86 && HWY_TARGET <= HWY_AVX3)
  Vec2<D> s = AddDF(df, ConvertTo(df, e),
				MulDF(df, x, Create2(df, Set(df, 2.8853900432586669922), Set(df, 3.2734474483568488616e-08))));
#else
  Vec2<D> s = AddDF(df, e,
				MulDF(df, x, Create2(df, Set(df, 2.8853900432586669922), Set(df, 3.2734474483568488616e-08))));
#endif

  s = AddDF(df, s, Mul(Mul(x2, Get2<0>(x)), t));

  Vec<D> r = Add(Get2<0>(s), Get2<1>(s));

#if !(HWY_ARCH_X86 && HWY_TARGET <= HWY_AVX3)
  r = IfThenElse(RebindMask(df, Eq(d, Inf(df))), Set(df, InfDouble), r);
  r = IfThenElse(RebindMask(df, Or(Lt(d, Set(df, 0)), IsNaN(d))), Set(df, NanDouble), r);
  r = IfThenElse(RebindMask(df, Eq(d, Set(df, 0))), Set(df, -InfDouble), r);
#else
  r = vfixup_vf_vf_vf_vi2_i(r, d, Set(di, (4 << (2*4)) | (3 << (4*4)) | (5 << (5*4)) | (2 << (6*4))), 0);
#endif
  
  return r;
}

// Computes sin(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:969 xsinf_u1
template<class D>
HWY_INLINE Vec<D> Sin(const D df, Vec<D> d) {
  RebindToUnsigned<D> du;
  RebindToSigned<D> di;
  
  Vec<RebindToSigned<D>> q;
  Vec<D> u, v;
  Vec2<D> s, t, x;

  u = Round(Mul(d, Set(df, OneOverPi)));
  q = NearestInt(u);
  v = MulAdd(u, Set(df, -PiA2f), d);
  s = AddDF(df, v, Mul(u, Set(df, -PiB2f)));
  s = AddFastDF(df, s, Mul(u, Set(df, -PiC2f)));
  Mask<D> g = Lt(Abs(d), Set(df, TrigRangeMax2));

  if (!HWY_LIKELY(AllTrue(df, RebindMask(df, g)))) {
    Vec3<D> dfi = PayneHanekReduction(df, d);
    Vec<RebindToSigned<D>> q2 = And(BitCast(di, Get3<2>(dfi)), Set(di, 3));
    q2 = Add(Add(q2, q2), IfThenElse(RebindMask(di, Gt(Get2<0>(Create2(df, Get3<0>(dfi), Get3<1>(dfi))), Set(df, 0))), Set(di, 2), Set(di, 1)));
    q2 = ShiftRight<2>(q2);
    Mask<D> o = RebindMask(df, Eq(And(BitCast(di, Get3<2>(dfi)), Set(di, 1)), Set(di, 1)));
    Vec2<D> x = Create2(df, MulSignBit(df, Set(df, 3.1415927410125732422f*-0.5), Get2<0>(Create2(df, Get3<0>(dfi), Get3<1>(dfi)))), MulSignBit(df, Set(df, -8.7422776573475857731e-08f*-0.5), Get2<0>(Create2(df, Get3<0>(dfi), Get3<1>(dfi)))));
    x = AddDF(df, Create2(df, Get3<0>(dfi), Get3<1>(dfi)), x);
    dfi = Set3<0>(Set3<1>(dfi, Get2<1>(IfThenElse(df, o, x, Create2(df, Get3<0>(dfi), Get3<1>(dfi))))), Get2<0>(IfThenElse(df, o, x, Create2(df, Get3<0>(dfi), Get3<1>(dfi)))));
    t = NormalizeDF(df, Create2(df, Get3<0>(dfi), Get3<1>(dfi)));

    t = Set2<0>(t, BitCast(df, IfThenElse(RebindMask(du, Or(IsInf(d), IsNaN(d))), Set(du, -1), BitCast(du, Get2<0>(t)))));

    q = IfThenElse(RebindMask(di, g), q, q2);
    s = IfThenElse(df, g, s, t);
  }

  t = s;
  s = SquareDF(df, s);

  u = Set(df, 2.6083159809786593541503e-06f);
  u = MulAdd(u, Get2<0>(s), Set(df, -0.0001981069071916863322258f));
  u = MulAdd(u, Get2<0>(s), Set(df, 0.00833307858556509017944336f));

  x = AddFastDF(df, Set(df, 1), MulDF(df, AddFastDF(df, Set(df, -0.166666597127914428710938f), Mul(u, Get2<0>(s))), s));

  u = MulDF_float(df, t, x);

  u = BitCast(df, Xor(IfThenElseZero(RebindMask(du, RebindMask(df, Eq(And(q, Set(di, 1)), Set(di, 1)))), BitCast(du, Set(df, -0.0))), BitCast(du, u)));

  u = IfThenElse(RebindMask(df, Eq(BitCast(du, d), Set(du, 0x80000000))), d, u);

  return u; // #if !defined(DETERMINISTIC)
}

// Computes cos(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:1067 xcosf_u1
template<class D>
HWY_INLINE Vec<D> Cos(const D df, Vec<D> d) {
  RebindToUnsigned<D> du;
  RebindToSigned<D> di;
  
  Vec<RebindToSigned<D>> q;
  Vec<D> u;
  Vec2<D> s, t, x;

  Vec<D> dq = MulAdd(Round(MulAdd(d, Set(df, OneOverPi), Set(df, -0.5f))), Set(df, 2), Set(df, 1));
  q = NearestInt(dq);
  s = AddDF(df, d, Mul(dq, Set(df, -PiA2f*0.5f)));
  s = AddDF(df, s, Mul(dq, Set(df, -PiB2f*0.5f)));
  s = AddDF(df, s, Mul(dq, Set(df, -PiC2f*0.5f)));
  Mask<D> g = Lt(Abs(d), Set(df, TrigRangeMax2));

  if (!HWY_LIKELY(AllTrue(df, RebindMask(df, g)))) {
    Vec3<D> dfi = PayneHanekReduction(df, d);
    Vec<RebindToSigned<D>> q2 = And(BitCast(di, Get3<2>(dfi)), Set(di, 3));
    q2 = Add(Add(q2, q2), IfThenElse(RebindMask(di, Gt(Get2<0>(Create2(df, Get3<0>(dfi), Get3<1>(dfi))), Set(df, 0))), Set(di, 8), Set(di, 7)));
    q2 = ShiftRight<1>(q2);
    Mask<D> o = RebindMask(df, Eq(And(BitCast(di, Get3<2>(dfi)), Set(di, 1)), Set(di, 0)));
    Vec<D> y = IfThenElse(RebindMask(df, Gt(Get2<0>(Create2(df, Get3<0>(dfi), Get3<1>(dfi))), Set(df, 0))), Set(df, 0), Set(df, -1));
    Vec2<D> x = Create2(df, MulSignBit(df, Set(df, 3.1415927410125732422f*-0.5), y), MulSignBit(df, Set(df, -8.7422776573475857731e-08f*-0.5), y));
    x = AddDF(df, Create2(df, Get3<0>(dfi), Get3<1>(dfi)), x);
    dfi = Set3<0>(Set3<1>(dfi, Get2<1>(IfThenElse(df, o, x, Create2(df, Get3<0>(dfi), Get3<1>(dfi))))), Get2<0>(IfThenElse(df, o, x, Create2(df, Get3<0>(dfi), Get3<1>(dfi)))));
    t = NormalizeDF(df, Create2(df, Get3<0>(dfi), Get3<1>(dfi)));

    t = Set2<0>(t, BitCast(df, IfThenElse(RebindMask(du, Or(IsInf(d), IsNaN(d))), Set(du, -1), BitCast(du, Get2<0>(t)))));

    q = IfThenElse(RebindMask(di, g), q, q2);
    s = IfThenElse(df, g, s, t);
  }

  t = s;
  s = SquareDF(df, s);

  u = Set(df, 2.6083159809786593541503e-06f);
  u = MulAdd(u, Get2<0>(s), Set(df, -0.0001981069071916863322258f));
  u = MulAdd(u, Get2<0>(s), Set(df, 0.00833307858556509017944336f));

  x = AddFastDF(df, Set(df, 1), MulDF(df, AddFastDF(df, Set(df, -0.166666597127914428710938f), Mul(u, Get2<0>(s))), s));

  u = MulDF_float(df, t, x);

  u = BitCast(df, Xor(IfThenElseZero(RebindMask(du, RebindMask(df, Eq(And(q, Set(di, 2)), Set(di, 0)))), BitCast(du, Set(df, -0.0))), BitCast(du, u)));
  
  return u; // #if !defined(DETERMINISTIC)
}

// Computes tan(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:1635 xtanf_u1
template<class D>
HWY_INLINE Vec<D> Tan(const D df, Vec<D> d) {
  RebindToUnsigned<D> du;
  RebindToSigned<D> di;
  
  Vec<RebindToSigned<D>> q;
  Vec<D> u, v;
  Vec2<D> s, t, x;
  Mask<D> o;

  u = Round(Mul(d, Set(df, 2 * OneOverPi)));
  q = NearestInt(u);
  v = MulAdd(u, Set(df, -PiA2f*0.5f), d);
  s = AddDF(df, v, Mul(u, Set(df, -PiB2f*0.5f)));
  s = AddFastDF(df, s, Mul(u, Set(df, -PiC2f*0.5f)));
  Mask<D> g = Lt(Abs(d), Set(df, TrigRangeMax2));

  if (!HWY_LIKELY(AllTrue(df, RebindMask(df, g)))) {
    Vec3<D> dfi = PayneHanekReduction(df, d);
    t = Create2(df, Get3<0>(dfi), Get3<1>(dfi));
    o = Or(IsInf(d), IsNaN(d));
    t = Set2<0>(t, BitCast(df, IfThenElse(RebindMask(du, o), Set(du, -1), BitCast(du, Get2<0>(t)))));
    t = Set2<1>(t, BitCast(df, IfThenElse(RebindMask(du, o), Set(du, -1), BitCast(du, Get2<1>(t)))));
    q = IfThenElse(RebindMask(di, g), q, BitCast(di, Get3<2>(dfi)));
    s = IfThenElse(df, g, s, t);
  }

  o = RebindMask(df, Eq(And(q, Set(di, 1)), Set(di, 1)));
  Vec<RebindToUnsigned<D>> n = IfThenElseZero(RebindMask(du, o), BitCast(du, Set(df, -0.0)));
  s = Set2<0>(s, BitCast(df, Xor(BitCast(du, Get2<0>(s)), n)));
  s = Set2<1>(s, BitCast(df, Xor(BitCast(du, Get2<1>(s)), n)));

  t = s;
  s = SquareDF(df, s);
  s = NormalizeDF(df, s);

  u = Set(df, 0.00446636462584137916564941f);
  u = MulAdd(u, Get2<0>(s), Set(df, -8.3920182078145444393158e-05f));
  u = MulAdd(u, Get2<0>(s), Set(df, 0.0109639242291450500488281f));
  u = MulAdd(u, Get2<0>(s), Set(df, 0.0212360303848981857299805f));
  u = MulAdd(u, Get2<0>(s), Set(df, 0.0540687143802642822265625f));

  x = AddFastDF(df, Set(df, 0.133325666189193725585938f), Mul(u, Get2<0>(s)));
  x = AddFastDF(df, Set(df, 1), MulDF(df, AddFastDF(df, Set(df, 0.33333361148834228515625f), MulDF(df, s, x)), s));
  x = MulDF(df, t, x);

  x = IfThenElse(df, o, RecDF(df, x), x);

  u = Add(Get2<0>(x), Get2<1>(x));

  u = IfThenElse(RebindMask(df, Eq(BitCast(du, d), Set(du, 0x80000000))), d, u);
  
  return u; // #if !defined(DETERMINISTIC)
}

// Computes sin(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:630 xsinf
template<class D>
HWY_INLINE Vec<D> SinFast(const D df, Vec<D> d) {
  RebindToUnsigned<D> du;
  RebindToSigned<D> di;
  
  Vec<RebindToSigned<D>> q;
  Vec<D> u, s, r = d;

  q = NearestInt(Mul(d, Set(df, (float)OneOverPi)));
  u = ConvertTo(df, q);
  d = MulAdd(u, Set(df, -PiA2f), d);
  d = MulAdd(u, Set(df, -PiB2f), d);
  d = MulAdd(u, Set(df, -PiC2f), d);
  Mask<D> g = Lt(Abs(r), Set(df, TrigRangeMax2));

  if (!HWY_LIKELY(AllTrue(df, RebindMask(df, g)))) {
    s = ConvertTo(df, q);
    u = MulAdd(s, Set(df, -PiAf), r);
    u = MulAdd(s, Set(df, -PiBf), u);
    u = MulAdd(s, Set(df, -PiCf), u);
    u = MulAdd(s, Set(df, -PiDf), u);

    d = IfThenElse(RebindMask(df, g), d, u);
    g = Lt(Abs(r), Set(df, TrigRangeMax));

    if (!HWY_LIKELY(AllTrue(df, RebindMask(df, g)))) {
      Vec3<D> dfi = PayneHanekReduction(df, r);
      Vec<RebindToSigned<D>> q2 = And(BitCast(di, Get3<2>(dfi)), Set(di, 3));
      q2 = Add(Add(q2, q2), IfThenElse(RebindMask(di, Gt(Get2<0>(Create2(df, Get3<0>(dfi), Get3<1>(dfi))), Set(df, 0))), Set(di, 2), Set(di, 1)));
      q2 = ShiftRight<2>(q2);
      Mask<D> o = RebindMask(df, Eq(And(BitCast(di, Get3<2>(dfi)), Set(di, 1)), Set(di, 1)));
      Vec2<D> x = Create2(df, MulSignBit(df, Set(df, 3.1415927410125732422f*-0.5), Get2<0>(Create2(df, Get3<0>(dfi), Get3<1>(dfi)))), MulSignBit(df, Set(df, -8.7422776573475857731e-08f*-0.5), Get2<0>(Create2(df, Get3<0>(dfi), Get3<1>(dfi)))));
      x = AddDF(df, Create2(df, Get3<0>(dfi), Get3<1>(dfi)), x);
      dfi = Set3<0>(Set3<1>(dfi, Get2<1>(IfThenElse(df, o, x, Create2(df, Get3<0>(dfi), Get3<1>(dfi))))), Get2<0>(IfThenElse(df, o, x, Create2(df, Get3<0>(dfi), Get3<1>(dfi)))));
      u = Add(Get2<0>(Create2(df, Get3<0>(dfi), Get3<1>(dfi))), Get2<1>(Create2(df, Get3<0>(dfi), Get3<1>(dfi))));

      u = BitCast(df, IfThenElse(RebindMask(du, Or(IsInf(r), IsNaN(r))), Set(du, -1), BitCast(du, u)));

      q = IfThenElse(RebindMask(di, g), q, q2);
      d = IfThenElse(RebindMask(df, g), d, u);
    }
  }

  s = Mul(d, d);

  d = BitCast(df, Xor(IfThenElseZero(RebindMask(du, RebindMask(df, Eq(And(q, Set(di, 1)), Set(di, 1)))), BitCast(du, Set(df, -0.0f))), BitCast(du, d)));

  u = Set(df, 2.6083159809786593541503e-06f);
  u = MulAdd(u, s, Set(df, -0.0001981069071916863322258f));
  u = MulAdd(u, s, Set(df, 0.00833307858556509017944336f));
  u = MulAdd(u, s, Set(df, -0.166666597127914428710938f));

  u = Add(Mul(s, Mul(u, d)), d);

  u = IfThenElse(RebindMask(df, Eq(BitCast(du, r), Set(du, 0x80000000))), r, u);

  return u; // #if !defined(DETERMINISTIC)
}

// Computes cos(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:736 xcosf
template<class D>
HWY_INLINE Vec<D> CosFast(const D df, Vec<D> d) {
  RebindToUnsigned<D> du;
  RebindToSigned<D> di;
  
  Vec<RebindToSigned<D>> q;
  Vec<D> u, s, r = d;

  q = NearestInt(Sub(Mul(d, Set(df, (float)OneOverPi)), Set(df, 0.5f)));
  q = Add(Add(q, q), Set(di, 1));
  u = ConvertTo(df, q);
  d = MulAdd(u, Set(df, -PiA2f*0.5f), d);
  d = MulAdd(u, Set(df, -PiB2f*0.5f), d);
  d = MulAdd(u, Set(df, -PiC2f*0.5f), d);
  Mask<D> g = Lt(Abs(r), Set(df, TrigRangeMax2));

  if (!HWY_LIKELY(AllTrue(df, RebindMask(df, g)))) {
    s = ConvertTo(df, q);
    u = MulAdd(s, Set(df, -PiAf*0.5f), r);
    u = MulAdd(s, Set(df, -PiBf*0.5f), u);
    u = MulAdd(s, Set(df, -PiCf*0.5f), u);
    u = MulAdd(s, Set(df, -PiDf*0.5f), u);

    d = IfThenElse(RebindMask(df, g), d, u);
    g = Lt(Abs(r), Set(df, TrigRangeMax));

    if (!HWY_LIKELY(AllTrue(df, RebindMask(df, g)))) {
      Vec3<D> dfi = PayneHanekReduction(df, r);
      Vec<RebindToSigned<D>> q2 = And(BitCast(di, Get3<2>(dfi)), Set(di, 3));
      q2 = Add(Add(q2, q2), IfThenElse(RebindMask(di, Gt(Get2<0>(Create2(df, Get3<0>(dfi), Get3<1>(dfi))), Set(df, 0))), Set(di, 8), Set(di, 7)));
      q2 = ShiftRight<1>(q2);
      Mask<D> o = RebindMask(df, Eq(And(BitCast(di, Get3<2>(dfi)), Set(di, 1)), Set(di, 0)));
      Vec<D> y = IfThenElse(RebindMask(df, Gt(Get2<0>(Create2(df, Get3<0>(dfi), Get3<1>(dfi))), Set(df, 0))), Set(df, 0), Set(df, -1));
      Vec2<D> x = Create2(df, MulSignBit(df, Set(df, 3.1415927410125732422f*-0.5), y), MulSignBit(df, Set(df, -8.7422776573475857731e-08f*-0.5), y));
      x = AddDF(df, Create2(df, Get3<0>(dfi), Get3<1>(dfi)), x);
      dfi = Set3<0>(Set3<1>(dfi, Get2<1>(IfThenElse(df, o, x, Create2(df, Get3<0>(dfi), Get3<1>(dfi))))), Get2<0>(IfThenElse(df, o, x, Create2(df, Get3<0>(dfi), Get3<1>(dfi)))));
      u = Add(Get2<0>(Create2(df, Get3<0>(dfi), Get3<1>(dfi))), Get2<1>(Create2(df, Get3<0>(dfi), Get3<1>(dfi))));

      u = BitCast(df, IfThenElse(RebindMask(du, Or(IsInf(r), IsNaN(r))), Set(du, -1), BitCast(du, u)));

      q = IfThenElse(RebindMask(di, g), q, q2);
      d = IfThenElse(RebindMask(df, g), d, u);
    }
  }

  s = Mul(d, d);

  d = BitCast(df, Xor(IfThenElseZero(RebindMask(du, RebindMask(df, Eq(And(q, Set(di, 2)), Set(di, 0)))), BitCast(du, Set(df, -0.0f))), BitCast(du, d)));

  u = Set(df, 2.6083159809786593541503e-06f);
  u = MulAdd(u, s, Set(df, -0.0001981069071916863322258f));
  u = MulAdd(u, s, Set(df, 0.00833307858556509017944336f));
  u = MulAdd(u, s, Set(df, -0.166666597127914428710938f));

  u = Add(Mul(s, Mul(u, d)), d);

  return u; // #if !defined(DETERMINISTIC)
}

// Computes tan(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:845 xtanf
template<class D>
HWY_INLINE Vec<D> TanFast(const D df, Vec<D> d) {
  RebindToUnsigned<D> du;
  RebindToSigned<D> di;
  
  Vec<RebindToSigned<D>> q;
  Mask<D> o;
  Vec<D> u, s, x;

  q = NearestInt(Mul(d, Set(df, (float)(2 * OneOverPi))));
  u = ConvertTo(df, q);
  x = MulAdd(u, Set(df, -PiA2f*0.5f), d);
  x = MulAdd(u, Set(df, -PiB2f*0.5f), x);
  x = MulAdd(u, Set(df, -PiC2f*0.5f), x);
  Mask<D> g = Lt(Abs(d), Set(df, TrigRangeMax2*0.5f));

  if (!HWY_LIKELY(AllTrue(df, RebindMask(df, g)))) {
    Vec<RebindToSigned<D>> q2 = NearestInt(Mul(d, Set(df, (float)(2 * OneOverPi))));
    s = ConvertTo(df, q);
    u = MulAdd(s, Set(df, -PiAf*0.5f), d);
    u = MulAdd(s, Set(df, -PiBf*0.5f), u);
    u = MulAdd(s, Set(df, -PiCf*0.5f), u);
    u = MulAdd(s, Set(df, -PiDf*0.5f), u);

    q = IfThenElse(RebindMask(di, g), q, q2);
    x = IfThenElse(RebindMask(df, g), x, u);
    g = Lt(Abs(d), Set(df, TrigRangeMax));

    if (!HWY_LIKELY(AllTrue(df, RebindMask(df, g)))) {
      Vec3<D> dfi = PayneHanekReduction(df, d);
      u = Add(Get2<0>(Create2(df, Get3<0>(dfi), Get3<1>(dfi))), Get2<1>(Create2(df, Get3<0>(dfi), Get3<1>(dfi))));
      u = BitCast(df, IfThenElse(RebindMask(du, Or(IsInf(d), IsNaN(d))), Set(du, -1), BitCast(du, u)));
      u = IfThenElse(RebindMask(df, Eq(BitCast(du, d), Set(du, 0x80000000))), d, u);
      q = IfThenElse(RebindMask(di, g), q, BitCast(di, Get3<2>(dfi)));
      x = IfThenElse(RebindMask(df, g), x, u);
    }
  }

  s = Mul(x, x);

  o = RebindMask(df, Eq(And(q, Set(di, 1)), Set(di, 1)));
  x = BitCast(df, Xor(IfThenElseZero(RebindMask(du, o), BitCast(du, Set(df, -0.0f))), BitCast(du, x)));

#if HWY_ARCH_ARM && HWY_TARGET >= HWY_NEON
  u = Set(df, 0.00927245803177356719970703f);
  u = MulAdd(u, s, Set(df, 0.00331984995864331722259521f));
  u = MulAdd(u, s, Set(df, 0.0242998078465461730957031f));
  u = MulAdd(u, s, Set(df, 0.0534495301544666290283203f));
  u = MulAdd(u, s, Set(df, 0.133383005857467651367188f));
  u = MulAdd(u, s, Set(df, 0.333331853151321411132812f));
#else
  Vec<D> s2 = Mul(s, s), s4 = Mul(s2, s2);
  u = Estrin(s, s2, s4, Set(df, 0.333331853151321411132812f), Set(df, 0.133383005857467651367188f), Set(df, 0.0534495301544666290283203f), Set(df, 0.0242998078465461730957031f), Set(df, 0.00331984995864331722259521f), Set(df, 0.00927245803177356719970703f));
#endif

  u = MulAdd(s, Mul(u, x), x);

  u = IfThenElse(RebindMask(df, o), Div(Set(df, 1.0), u), u);

  return u; // #if !defined(DETERMINISTIC)
}

// Computes sinh(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:2447 xsinhf
template<class D>
HWY_INLINE Vec<D> Sinh(const D df, Vec<D> x) {
  RebindToUnsigned<D> du;
  
  Vec<D> y = Abs(x);
  Vec2<D> d = ExpDF(df, Create2(df, y, Set(df, 0)));
  d = SubDF(df, d, RecDF(df, d));
  y = Mul(Add(Get2<0>(d), Get2<1>(d)), Set(df, 0.5));

  y = IfThenElse(RebindMask(df, Or(Gt(Abs(x), Set(df, 89)), IsNaN(y))), Set(df, InfFloat), y);
  y = MulSignBit(df, y, x);
  y = BitCast(df, IfThenElse(RebindMask(du, IsNaN(x)), Set(du, -1), BitCast(du, y)));

  return y;
}

// Computes cosh(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:2461 xcoshf
template<class D>
HWY_INLINE Vec<D> Cosh(const D df, Vec<D> x) {
  RebindToUnsigned<D> du;
  
  Vec<D> y = Abs(x);
  Vec2<D> d = ExpDF(df, Create2(df, y, Set(df, 0)));
  d = AddFastDF(df, d, RecDF(df, d));
  y = Mul(Add(Get2<0>(d), Get2<1>(d)), Set(df, 0.5));

  y = IfThenElse(RebindMask(df, Or(Gt(Abs(x), Set(df, 89)), IsNaN(y))), Set(df, InfFloat), y);
  y = BitCast(df, IfThenElse(RebindMask(du, IsNaN(x)), Set(du, -1), BitCast(du, y)));

  return y;
}

// Computes tanh(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:2474 xtanhf
template<class D>
HWY_INLINE Vec<D> Tanh(const D df, Vec<D> x) {
  RebindToUnsigned<D> du;
  
  Vec<D> y = Abs(x);
  Vec2<D> d = ExpDF(df, Create2(df, y, Set(df, 0)));
  Vec2<D> e = RecDF(df, d);
  d = DivDF(df, AddFastDF(df, d, NegDF(df, e)), AddFastDF(df, d, e));
  y = Add(Get2<0>(d), Get2<1>(d));

  y = IfThenElse(RebindMask(df, Or(Gt(Abs(x), Set(df, 8.664339742f)), IsNaN(y))), Set(df, 1.0f), y);
  y = MulSignBit(df, y, x);
  y = BitCast(df, IfThenElse(RebindMask(du, IsNaN(x)), Set(du, -1), BitCast(du, y)));

  return y;
}

// Computes sinh(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:2489 xsinhf_u35
template<class D>
HWY_INLINE Vec<D> SinhFast(const D df, Vec<D> x) {
  RebindToUnsigned<D> du;
  
  Vec<D> e = Expm1Fast(df, Abs(x));
  Vec<D> y = Div(Add(e, Set(df, 2)), Add(e, Set(df, 1)));
  y = Mul(y, Mul(Set(df, 0.5f), e));

  y = IfThenElse(RebindMask(df, Or(Gt(Abs(x), Set(df, 88)), IsNaN(y))), Set(df, InfFloat), y);
  y = MulSignBit(df, y, x);
  y = BitCast(df, IfThenElse(RebindMask(du, IsNaN(x)), Set(du, -1), BitCast(du, y)));

  return y;
}

// Computes cosh(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:2502 xcoshf_u35
template<class D>
HWY_INLINE Vec<D> CoshFast(const D df, Vec<D> x) {
  RebindToUnsigned<D> du;
  
  Vec<D> e = sleef::Exp(df, Abs(x));
  Vec<D> y = MulAdd(Set(df, 0.5f), e, Div(Set(df, 0.5), e));

  y = IfThenElse(RebindMask(df, Or(Gt(Abs(x), Set(df, 88)), IsNaN(y))), Set(df, InfFloat), y);
  y = BitCast(df, IfThenElse(RebindMask(du, IsNaN(x)), Set(du, -1), BitCast(du, y)));

  return y;
}

// Computes tanh(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:2513 xtanhf_u35
template<class D>
HWY_INLINE Vec<D> TanhFast(const D df, Vec<D> x) {
  RebindToUnsigned<D> du;
  
  Vec<D> d = Expm1Fast(df, Mul(Set(df, 2), Abs(x)));
  Vec<D> y = Div(d, Add(Set(df, 2), d));

  y = IfThenElse(RebindMask(df, Or(Gt(Abs(x), Set(df, 8.664339742f)), IsNaN(y))), Set(df, 1.0f), y);
  y = MulSignBit(df, y, x);
  y = BitCast(df, IfThenElse(RebindMask(du, IsNaN(x)), Set(du, -1), BitCast(du, y)));

  return y;
}

// Computes acos(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:1948 xacosf_u1
template<class D>
HWY_INLINE Vec<D> Acos(const D df, Vec<D> d) {
  Mask<D> o = Lt(Abs(d), Set(df, 0.5f));
  Vec<D> x2 = IfThenElse(RebindMask(df, o), Mul(d, d), Mul(Sub(Set(df, 1), Abs(d)), Set(df, 0.5f))), u;
  Vec2<D> x = IfThenElse(df, o, Create2(df, Abs(d), Set(df, 0)), SqrtDF(df, x2));
  x = IfThenElse(df, Eq(Abs(d), Set(df, 1.0f)), Create2(df, Set(df, 0), Set(df, 0)), x);

  u = Set(df, +0.4197454825e-1);
  u = MulAdd(u, x2, Set(df, +0.2424046025e-1));
  u = MulAdd(u, x2, Set(df, +0.4547423869e-1));
  u = MulAdd(u, x2, Set(df, +0.7495029271e-1));
  u = MulAdd(u, x2, Set(df, +0.1666677296e+0));
  u = Mul(u, Mul(x2, Get2<0>(x)));

  Vec2<D> y = SubDF(df, Create2(df, Set(df, 3.1415927410125732422f/2), Set(df, -8.7422776573475857731e-08f/2)),
				 AddFastDF(df, MulSignBit(df, Get2<0>(x), d), MulSignBit(df, u, d)));
  x = AddFastDF(df, x, u);

  y = IfThenElse(df, o, y, ScaleDF(df, x, Set(df, 2)));
  
  y = IfThenElse(df, AndNot(o, Lt(d, Set(df, 0))),
			  SubDF(df, Create2(df, Set(df, 3.1415927410125732422f), Set(df, -8.7422776573475857731e-08f)), y), y);

  return Add(Get2<0>(y), Get2<1>(y));
}

// Computes asin(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:1928 xasinf_u1
template<class D>
HWY_INLINE Vec<D> Asin(const D df, Vec<D> d) {
  Mask<D> o = Lt(Abs(d), Set(df, 0.5f));
  Vec<D> x2 = IfThenElse(RebindMask(df, o), Mul(d, d), Mul(Sub(Set(df, 1), Abs(d)), Set(df, 0.5f))), u;
  Vec2<D> x = IfThenElse(df, o, Create2(df, Abs(d), Set(df, 0)), SqrtDF(df, x2));
  x = IfThenElse(df, Eq(Abs(d), Set(df, 1.0f)), Create2(df, Set(df, 0), Set(df, 0)), x);

  u = Set(df, +0.4197454825e-1);
  u = MulAdd(u, x2, Set(df, +0.2424046025e-1));
  u = MulAdd(u, x2, Set(df, +0.4547423869e-1));
  u = MulAdd(u, x2, Set(df, +0.7495029271e-1));
  u = MulAdd(u, x2, Set(df, +0.1666677296e+0));
  u = Mul(u, Mul(x2, Get2<0>(x)));

  Vec2<D> y = SubDF(df, SubDF(df, Create2(df, Set(df, 3.1415927410125732422f/4), Set(df, -8.7422776573475857731e-08f/4)), x), u);
  
  Vec<D> r = IfThenElse(RebindMask(df, o), Add(u, Get2<0>(x)), Mul(Add(Get2<0>(y), Get2<1>(y)), Set(df, 2)));
  return MulSignBit(df, r, d);
}

// Computes asinh(x) with 1 ULP accuracy
// Translated from libm/sleefsimdsp.c:2554 xasinhf
template<class D>
HWY_INLINE Vec<D> Asinh(const D df, Vec<D> x) {
  RebindToUnsigned<D> du;
  
  Vec<D> y = Abs(x);
  Mask<D> o = Gt(y, Set(df, 1));
  Vec2<D> d;
  
  d = IfThenElse(df, o, RecDF(df, x), Create2(df, y, Set(df, 0)));
  d = SqrtDF(df, AddDF(df, SquareDF(df, d), Set(df, 1)));
  d = IfThenElse(df, o, MulDF(df, d, y), d);

  d = LogFastDF(df, NormalizeDF(df, AddDF(df, d, x)));
  y = Add(Get2<0>(d), Get2<1>(d));

  y = IfThenElse(RebindMask(df, Or(Gt(Abs(x), Set(df, SqrtFloatMax)), IsNaN(y))), MulSignBit(df, Set(df, InfFloat), x), y);
  y = BitCast(df, IfThenElse(RebindMask(du, IsNaN(x)), Set(du, -1), BitCast(du, y)));
  y = IfThenElse(RebindMask(df, Eq(BitCast(du, x), Set(du, 0x80000000))), Set(df, -0.0), y);

  return y;
}

// Computes acos(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:1847 xacosf
template<class D>
HWY_INLINE Vec<D> AcosFast(const D df, Vec<D> d) {
  Mask<D> o = Lt(Abs(d), Set(df, 0.5f));
  Vec<D> x2 = IfThenElse(RebindMask(df, o), Mul(d, d), Mul(Sub(Set(df, 1), Abs(d)), Set(df, 0.5f))), u;
  Vec<D> x = IfThenElse(RebindMask(df, o), Abs(d), Sqrt(x2));
  x = IfThenElse(RebindMask(df, Eq(Abs(d), Set(df, 1.0f))), Set(df, 0), x);

  u = Set(df, +0.4197454825e-1);
  u = MulAdd(u, x2, Set(df, +0.2424046025e-1));
  u = MulAdd(u, x2, Set(df, +0.4547423869e-1));
  u = MulAdd(u, x2, Set(df, +0.7495029271e-1));
  u = MulAdd(u, x2, Set(df, +0.1666677296e+0));
  u = Mul(u, Mul(x2, x));

  Vec<D> y = Sub(Set(df, 3.1415926535897932f/2), Add(MulSignBit(df, x, d), MulSignBit(df, u, d)));
  x = Add(x, u);
  Vec<D> r = IfThenElse(RebindMask(df, o), y, Mul(x, Set(df, 2)));
  return IfThenElse(RebindMask(df, AndNot(o, Lt(d, Set(df, 0)))), Get2<0>(AddFastDF(df, Create2(df, Set(df, 3.1415927410125732422f), Set(df, -8.7422776573475857731e-08f)),
							  Neg(r))), r);
}

// Computes asin(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:1831 xasinf
template<class D>
HWY_INLINE Vec<D> AsinFast(const D df, Vec<D> d) {
  Mask<D> o = Lt(Abs(d), Set(df, 0.5f));
  Vec<D> x2 = IfThenElse(RebindMask(df, o), Mul(d, d), Mul(Sub(Set(df, 1), Abs(d)), Set(df, 0.5f)));
  Vec<D> x = IfThenElse(RebindMask(df, o), Abs(d), Sqrt(x2)), u;

  u = Set(df, +0.4197454825e-1);
  u = MulAdd(u, x2, Set(df, +0.2424046025e-1));
  u = MulAdd(u, x2, Set(df, +0.4547423869e-1));
  u = MulAdd(u, x2, Set(df, +0.7495029271e-1));
  u = MulAdd(u, x2, Set(df, +0.1666677296e+0));
  u = MulAdd(u, Mul(x, x2), x);

  Vec<D> r = IfThenElse(RebindMask(df, o), u, MulAdd(u, Set(df, -2), Set(df, Pif/2)));
  return MulSignBit(df, r, d);
}

// Computes atan(x) with 3.5 ULP accuracy
// Translated from libm/sleefsimdsp.c:1743 xatanf
template<class D>
HWY_INLINE Vec<D> AtanFast(const D df, Vec<D> d) {
  RebindToUnsigned<D> du;
  RebindToSigned<D> di;
  
  Vec<D> s, t, u;
  Vec<RebindToSigned<D>> q;

  q = SignBitOrZero(df, d, Set(di, 2));
  s = Abs(d);

  q = IfThenElse(RebindMask(di, Lt(Set(df, 1.0f), s)), Add(q, Set(di, 1)), q);
  s = IfThenElse(RebindMask(df, Lt(Set(df, 1.0f), s)), Div(Set(df, 1.0), s), s);

  t = Mul(s, s);

  Vec<D> t2 = Mul(t, t), t4 = Mul(t2, t2);
  u = Estrin(t, t2, t4, Set(df, -0.333331018686294555664062f), Set(df, 0.199926957488059997558594f), Set(df, -0.142027363181114196777344f), Set(df, 0.106347933411598205566406f), Set(df, -0.0748900920152664184570312f), Set(df, 0.0425049886107444763183594f), Set(df, -0.0159569028764963150024414f), Set(df, 0.00282363896258175373077393f));

  t = MulAdd(s, Mul(t, u), s);

  t = IfThenElse(RebindMask(df, RebindMask(df, Eq(And(q, Set(di, 1)), Set(di, 1)))), Sub(Set(df, (float)(Pi/2)), t), t);

  t = BitCast(df, Xor(IfThenElseZero(RebindMask(du, RebindMask(df, Eq(And(q, Set(di, 2)), Set(di, 2)))), BitCast(du, Set(df, -0.0f))), BitCast(du, t)));

#if HWY_ARCH_ARM && HWY_TARGET >= HWY_NEON
  t = IfThenElse(RebindMask(df, IsInf(d)), MulSignBit(df, Set(df, 1.5874010519681994747517056f), d), t);
#endif

  return t;
}

// Computes acosh(x) with 1 ULP accuracy
// Translated from libm/sleefsimdsp.c:2575 xacoshf
template<class D>
HWY_INLINE Vec<D> Acosh(const D df, Vec<D> x) {
  RebindToUnsigned<D> du;
  
  Vec2<D> d = LogFastDF(df, AddDF(df, MulDF(df, SqrtDF(df, AddDF(df, x, Set(df, 1))), SqrtDF(df, AddDF(df, x, Set(df, -1)))), x));
  Vec<D> y = Add(Get2<0>(d), Get2<1>(d));

  y = IfThenElse(RebindMask(df, Or(Gt(Abs(x), Set(df, SqrtFloatMax)), IsNaN(y))), Set(df, InfFloat), y);

  y = BitCast(df, IfThenZeroElse(RebindMask(du, Eq(x, Set(df, 1.0f))), BitCast(du, y)));

  y = BitCast(df, IfThenElse(RebindMask(du, Lt(x, Set(df, 1.0f))), Set(du, -1), BitCast(du, y)));
  y = BitCast(df, IfThenElse(RebindMask(du, IsNaN(x)), Set(du, -1), BitCast(du, y)));

  return y;
}

// Computes atan(x) with 1.0 ULP accuracy
// Translated from libm/sleefsimdsp.c:1973 xatanf_u1
template<class D>
HWY_INLINE Vec<D> Atan(const D df, Vec<D> d) {
  Vec2<D> d2 = ATan2DF(df, Create2(df, Abs(d), Set(df, 0)), Create2(df, Set(df, 1), Set(df, 0)));
  Vec<D> r = Add(Get2<0>(d2), Get2<1>(d2));
  r = IfThenElse(RebindMask(df, IsInf(d)), Set(df, 1.570796326794896557998982), r);
  return MulSignBit(df, r, d);
}

// Computes atanh(x) with 1 ULP accuracy
// Translated from libm/sleefsimdsp.c:2591 xatanhf
template<class D>
HWY_INLINE Vec<D> Atanh(const D df, Vec<D> x) {
  RebindToUnsigned<D> du;
  
  Vec<D> y = Abs(x);
  Vec2<D> d = LogFastDF(df, DivDF(df, AddDF(df, Set(df, 1), y), AddDF(df, Set(df, 1), Neg(y))));
  y = BitCast(df, IfThenElse(RebindMask(du, Gt(y, Set(df, 1.0))), Set(du, -1), BitCast(du, IfThenElse(RebindMask(df, Eq(y, Set(df, 1.0))), Set(df, InfFloat), Mul(Add(Get2<0>(d), Get2<1>(d)), Set(df, 0.5))))));

  y = BitCast(df, IfThenElse(RebindMask(du, Or(IsInf(x), IsNaN(y))), Set(du, -1), BitCast(du, y)));
  y = MulSignBit(df, y, x);
  y = BitCast(df, IfThenElse(RebindMask(du, IsNaN(x)), Set(du, -1), BitCast(du, y)));

  return y;
}

}  // namespace sleef
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // HIGHWAY_HWY_CONTRIB_MATH_MATH_INL_H_
