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

#if defined(HIGHWAY_HWY_EOTF_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_EOTF_INL_H_
#undef HIGHWAY_HWY_EOTF_INL_H_
#else
#define HIGHWAY_HWY_EOTF_INL_H_
#endif

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace coder::HWY_NAMESPACE {

    static const float betaRec2020 = 0.018053968510807f;
    static const float alphaRec2020 = 1.09929682680944f;

    using namespace hwy;
    using namespace hwy::HWY_NAMESPACE;

    HWY_FAST_MATH_INLINE float bt2020GammaCorrection(float linear) {
        if (0 <= betaRec2020 && linear < betaRec2020) {
            return 4.5f * linear;
        } else if (betaRec2020 <= linear && linear < 1) {
            return alphaRec2020 * powf(linear, 0.45f) - (alphaRec2020 - 1.0f);
        }
        return linear;
    }

    HWY_FAST_MATH_INLINE float ToLinearPQ(float v, const float sdrReferencePoint) {
        float o = v;
        v = std::max(0.0f, v);
        const float m1 = (2610.0f / 4096.0f) / 4.0f;
        const float m2 = (2523.0f / 4096.0f) * 128.0f;
        const float c1 = 3424.0f / 4096.0f;
        const float c2 = (2413.0f / 4096.0f) * 32.0f;
        const float c3 = (2392.0f / 4096.0f) * 32.0f;
        float p = std::powf(v, 1.0f / m2);
        v = std::powf(max(p - c1, 0.0f) / (c2 - c3 * p), 1.0f / m1);
        v *= 10000.0f / sdrReferencePoint;
        return std::copysignf(v, o);
    }

    using hwy::HWY_NAMESPACE::Zero;
    using hwy::HWY_NAMESPACE::Vec;
    using hwy::HWY_NAMESPACE::TFromD;

    template<class DF, class V, HWY_IF_FLOAT(TFromD<DF>)>
    HWY_FAST_MATH_INLINE V
    HLGEotf(const DF df, V v) {
        v = Max(Zero(df), v);
        using VF32 = Vec<decltype(df)>;
        using T = hwy::HWY_NAMESPACE::TFromD<DF>;
        const VF32 a = Set(df, static_cast<T>(0.17883277f));
        const VF32 b = Set(df, static_cast<T>(0.28466892f));
        const VF32 c = Set(df, static_cast<T>(0.55991073f));
        const VF32 mm = Set(df, static_cast<T>(0.5f));
        const VF32 inversed3 = ApproximateReciprocal(Set(df, static_cast<T>(3.f)));
        const VF32 inversed12 = ApproximateReciprocal(Set(df, static_cast<T>(12.0f)));
        const auto cmp = v < mm;
        auto branch1 = Mul(Mul(v, v), inversed3);
        auto branch2 = Mul(coder::HWY_NAMESPACE::Exp2f(df, Add(Div(Sub(v, c), a), b)),
                           inversed12);
        return IfThenElse(cmp, branch1, branch2);
    }

    template<class D, class V, HWY_IF_FLOAT(TFromD<D>)>
    HWY_FAST_MATH_INLINE V ToLinearPQ(const D df, V v, const TFromD<D> sdrReferencePoint) {
        const V zeros = Zero(df);
        v = Max(zeros, v);
        using T = hwy::HWY_NAMESPACE::TFromD<D>;
        const T m1 = static_cast<T>((2610.0f / 4096.0f) / 4.0f);
        const T m2 = static_cast<T>((2523.0f / 4096.0f) * 128.0f);
        const V c1 = Set(df, static_cast<T>(3424.0f / 4096.0f));
        const V c2 = Set(df, static_cast<T>((2413.0f / 4096.0f) * 32.0f));
        const V c3 = Set(df, static_cast<T>((2392.0f / 4096.0f) * 32.0f));
        const V p1Power = Set(df, static_cast<T>(1.0f / m2));
        const V p2Power = Set(df, static_cast<T>(1.0f / m1));
        const V p3Power = Set(df, static_cast<T>(10000.0f / sdrReferencePoint));
        const V p = coder::HWY_NAMESPACE::Pow(df, v, p1Power);
        v = coder::HWY_NAMESPACE::Pow(df, Div(Max(Sub(p, c1), zeros), Sub(c2, Mul(c3, p))),
                                      p2Power);
        v = Mul(v, p3Power);
        return v;
    }

    template<class D, typename V = Vec<D>, HWY_IF_FLOAT(TFromD<D>)>
    HWY_FAST_MATH_INLINE V bt2020GammaCorrection(const D d, V color) {
        const V bt2020 = Set(d, betaRec2020);
        const V alpha2020 = Set(d, alphaRec2020);
        using T = hwy::HWY_NAMESPACE::TFromD<D>;
        const auto cmp = color < bt2020;
        const auto fourAndHalf = Set(d, static_cast<T>(4.5f));
        const auto branch1 = Mul(color, fourAndHalf);
        const V power1 = Set(d, static_cast<T>(0.45f));
        const V ones = Set(d, static_cast<T>(1.0f));
        const V power2 = Sub(alpha2020, ones);
        const auto branch2 =
                Sub(Mul(alpha2020, coder::HWY_NAMESPACE::Pow(d, color, power1)), power2);
        return IfThenElse(cmp, branch1, branch2);
    }

    template<class D, typename V = Vec<D>, HWY_IF_FLOAT(TFromD<D>)>
    HWY_FAST_MATH_INLINE V LinearITUR709ToITUR709(const D df, V value) {
        const auto minCurve = Set(df, static_cast<TFromD<D>>(0.018f));
        const auto minPowValue = Set(df, static_cast<TFromD<D>>(4.5f));
        const auto minLane = Mul(value, minPowValue);
        const auto subValue = Set(df, static_cast<TFromD<D>>(0.099f));
        const auto scalePower = Set(df, static_cast<TFromD<D>>(1.099f));
        const auto pwrValue = Set(df, static_cast<TFromD<D>>(0.45f));
        const auto maxLane = MulSub(coder::HWY_NAMESPACE::Pow(df, value, pwrValue), scalePower,
                                    subValue);
        return IfThenElse(value < minCurve, minLane, maxLane);
    }

    HWY_FAST_MATH_INLINE float SRGBToLinear(float v) {
        if (v <= 0.045f) {
            return v / 12.92f;
        } else {
            return std::powf((v + 0.055f) / 1.055f, 2.4f);
        }
    }

    HWY_FAST_MATH_INLINE float Rec709Eotf(float v) {
        if (v < 0.f) {
            return 0.f;
        } else if (v < 4.5f * 0.018053968510807f) {
            return v / 4.5f;
        } else if (v < 1.f) {
            return std::powf((v + 0.09929682680944f) / 1.09929682680944f, 1.f / 0.45f);
        }
        return 1.f;
    }

    HWY_FAST_MATH_INLINE float FromLinear709(float linear) {
        if (linear < 0.f) {
            return 0.f;
        } else if (linear < 0.018053968510807f) {
            return linear * 4.5f;
        } else if (linear < 1.f) {
            return 1.09929682680944f * std::powf(linear, 0.45f) - 0.09929682680944f;
        }
        return 1.f;
    }

    template<class D, HWY_IF_F32_D(D), typename T = TFromD<D>, typename V = VFromD<D>>
    HWY_FAST_MATH_INLINE V SRGBToLinear(D d, V v) {
        const auto lowerValueThreshold = Set(d, T(0.045f));
        const auto lowValueDivider = ApproximateReciprocal(Set(d, T(12.92f)));
        const auto lowMask = v <= lowerValueThreshold;
        const auto lowValue = Mul(v, lowValueDivider);
        const auto powerStatic = Set(d, T(2.4f));
        const auto addStatic = Set(d, T(0.055f));
        const auto scaleStatic = ApproximateReciprocal(Set(d, T(1.055f)));
        const auto highValue = Pow(d, Mul(Add(v, addStatic), scaleStatic), powerStatic);
        return IfThenElse(lowMask, lowValue, highValue);
    }

    HWY_FAST_MATH_INLINE float LinearSRGBTosRGB(const float linear) {
        if (linear <= 0.0031308f) {
            return 12.92f * linear;
        } else {
            return 1.055f * std::powf(linear, 1.0f / 2.4f) - 0.055f;
        }
    }

    template<class D, typename V = Vec<D>, HWY_IF_FLOAT(TFromD<D>)>
    HWY_FAST_MATH_INLINE V LinearSRGBTosRGB(const D df, V value) {
        const auto minCurve = Set(df, static_cast<TFromD<D>>(0.0031308f));
        const auto minPowValue = Set(df, static_cast<TFromD<D>>(12.92f));
        const auto minLane = Mul(value, minPowValue);
        const auto subValue = Set(df, static_cast<TFromD<D>>(0.055f));
        const auto scalePower = Set(df, static_cast<TFromD<D>>(1.055f));
        const auto pwrValue = Set(df, static_cast<TFromD<D>>(1.0f / 2.4f));
        const auto maxLane = MulSub(coder::HWY_NAMESPACE::Pow(df, value, pwrValue), scalePower,
                                    subValue);
        return IfThenElse(value <= minCurve, minLane, maxLane);
    }

    HWY_FAST_MATH_INLINE float LinearITUR709ToITUR709(const float linear) {
        if (linear <= 0.018f) {
            return 4.5f * linear;
        } else {
            return 1.099f * std::powf(linear, 0.45f) - 0.099f;
        }
    }

    template<class D, typename V = Vec<D>, HWY_IF_FLOAT(TFromD<D>)>
    HWY_FAST_MATH_INLINE V SMPTE428Eotf(const D df, V value) {
        const auto scale = Set(df, static_cast<const TFromD<D>>(1.f / 0.91655527974030934f));
        const auto twoPoint6 = Set(df, static_cast<TFromD<D>>(2.6f));
        const auto zeros = Zero(df);
        return Mul(coder::HWY_NAMESPACE::Pow(df, Max(value, zeros), twoPoint6), scale);
    }

    HWY_FAST_MATH_INLINE float SMPTE428Eotf(const float value) {
        return std::powf(std::max(value, 0.f), 2.6f) / 0.91655527974030934f;
    }

    HWY_FAST_MATH_INLINE float HLGEotf(float v) {
        v = std::max(0.0f, v);
        constexpr float a = 0.17883277f;
        constexpr float b = 0.28466892f;
        constexpr float c = 0.55991073f;
        if (v <= 0.5f)
            v = v * v / 3.0f;
        else
            v = (std::expf((v - c) / a) + b) / 12.f;
        return v;
    }

    template<class D, typename T = Vec<D>, HWY_IF_FLOAT(TFromD<D>)>
    HWY_FAST_MATH_INLINE T dciP3PQGammaCorrection(const D d, T color) {
        const auto pw = Set(d, 1.f / 2.6f);
        return coder::HWY_NAMESPACE::Pow(d, color, pw);
    }

    template<class D, typename T = Vec<D>, HWY_IF_FLOAT(TFromD<D>)>
    HWY_FAST_MATH_INLINE T gammaOtf(const D d, T color, TFromD<D> gamma) {
        const auto pw = Set(d, gamma);
        return coder::HWY_NAMESPACE::Pow(d, color, pw);
    }

    HWY_FAST_MATH_INLINE float dciP3PQGammaCorrection(float linear) {
        return std::powf(linear, 1.f / 2.6f);
    }

    HWY_FAST_MATH_INLINE float gammaOtf(float linear, const float gamma) {
        return std::powf(linear, gamma);
    }

    HWY_FAST_MATH_INLINE float Rec601Oetf(float intensity) {
        if (intensity < 0.018f) {
            return intensity * 4.5f;
        } else {
            return 1.099 * std::powf(intensity, 0.45f) - 0.099f;
        }
    }

    template<class D, typename T = Vec<D>, HWY_IF_FLOAT(TFromD<D>)>
    HWY_FAST_MATH_INLINE T Rec601Eotf(const D d, T intensity) {
        const auto topValue = Set(d, 0.081f);
        const auto fourAnd5 = Set(d, 4.5f);
        const auto lowMask = intensity < topValue;
        const auto lowValues = Div(intensity, fourAnd5);

        const auto addComp = Set(d, 0.099f);
        const auto div1099 = Set(d, 1.099f);
        const auto pwScale = Set(d, 1.0f / 0.45f);

        const auto high = coder::HWY_NAMESPACE::Pow(d, Div(Add(intensity, addComp), div1099),
                                                    pwScale);
        return IfThenElse(lowMask, lowValues, high);
    }

    HWY_FAST_MATH_INLINE float Rec601Eotf(float intensity) {
        if (intensity < 0.081f) {
            return intensity / 4.5f;
        } else {
            return std::powf((intensity + 0.099f) / 1.099f, 1.0f / 0.45f);
        }
    }

    HWY_FAST_MATH_INLINE float Smpte240Eotf(float gamma) {
        if (gamma < 0.f) {
            return 0.f;
        } else if (gamma < 4.f * 0.022821585529445f) {
            return gamma / 4.f;
        } else if (gamma < 1.f) {
            return std::powf((gamma + 0.111572195921731f) / 1.111572195921731f, 1.f / 0.45f);
        }
        return 1.f;
    }

    template<class D, typename T = Vec<D>, HWY_IF_FLOAT(TFromD<D>)>
    HWY_FAST_MATH_INLINE T Smpte240Eotf(const D d, T intensity) {
        const auto topValue = Set(d, 4.f * 0.022821585529445f);
        const auto lowValueDivider = Set(d, 4.f);
        const auto lowMask = intensity < topValue;
        const auto lowValues = Div(intensity, lowValueDivider);

        const auto addComp = Set(d, 0.111572195921731f);
        const auto div1099 = Set(d, 1.111572195921731f);
        const auto pwScale = Set(d, 1.0f / 0.45f);

        const auto high = coder::HWY_NAMESPACE::Pow(d, Div(Add(intensity, addComp), div1099),
                                                    pwScale);
        return IfThenElse(lowMask, lowValues, high);
    }

    HWY_FAST_MATH_INLINE float Smpte240Oetf(float linear) {
        if (linear < 0.f) {
            return 0.f;
        } else if (linear < 0.022821585529445f) {
            return linear * 4.f;
        } else if (linear < 1.f) {
            return 1.111572195921731f * std::powf(linear, 0.45f) - 0.111572195921731f;
        }
        return 1.f;
    }

    template<class D, HWY_IF_F32_D(D), typename T = TFromD<D>, typename V = VFromD<D>>
    HWY_FAST_MATH_INLINE V Rec709Eotf(D d, V v) {
        return Rec601Eotf(d, v);
    }

    HWY_FAST_MATH_INLINE float Log100Eotf(float gamma) {
        // The function is non-bijective so choose the middle of [0, 0.01].
        const float mid_interval = 0.01f / 2.f;
        return (gamma <= 0.0f) ? mid_interval
                               : std::powf(10.0f, 2.f * (std::min(gamma, 1.f) - 1.0f));
    }

    template<class D, HWY_IF_F32_D(D), typename T = TFromD<D>, typename V = VFromD<D>>
    HWY_FAST_MATH_INLINE V Log100Eotf(D d, V v) {
        // The function is non-bijective so choose the middle of [0, 0.01].
        const auto midInterval = Set(d, 0.01f / 2.f);
        const auto zeros = Zero(d);
        const auto zerosMask = v > zeros;
        const auto ones = Set(d, 1.f);
        const auto twos = Set(d, 2.f);
        const auto tens = Set(d, 10.f);
        const auto highPart = coder::HWY_NAMESPACE::Pow(d, tens,
                                                        Add(twos, Sub(Min(v, ones), ones)));
        return IfThenElseZero(zerosMask, highPart);
    }

    HWY_FAST_MATH_INLINE float Log100Oetf(float linear) {
        return (linear < 0.01f) ? 0.0f : 1.0f + std::log10f(std::min(linear, 1.f)) / 2.0f;
    }

    HWY_FAST_MATH_INLINE float Log100Sqrt10Eotf(float gamma) {
        // The function is non-bijective so choose the middle of [0, 0.00316227766f[.
        const float mid_interval = 0.00316227766f / 2.f;
        return (gamma <= 0.0f) ? mid_interval
                               : std::powf(10.0f, 2.5f * (std::min(gamma, 1.f) - 1.0f));
    }

    template<class D, HWY_IF_F32_D(D), typename T = TFromD<D>, typename V = VFromD<D>>
    HWY_FAST_MATH_INLINE V Log100Sqrt10Eotf(D d, V v) {
        // The function is non-bijective so choose the middle of [0, 0.01].
        const auto midInterval = Set(d, 0.00316227766f / 2.f);
        const auto zeros = Zero(d);
        const auto zerosMask = v > zeros;
        const auto ones = Set(d, 1.f);
        const auto twos = Set(d, 2.5f);
        const auto tens = Set(d, 10.f);
        const auto highPart = coder::HWY_NAMESPACE::Pow(d, tens,
                                                        Add(twos, Sub(Min(v, ones), ones)));
        return IfThenElseZero(zerosMask, highPart);
    }

    HWY_FAST_MATH_INLINE float Log100Sqrt10Oetf(float linear) {
        return (linear < 0.00316227766f) ? 0.0f
                                         : 1.0f + std::log10f(std::min(linear, 1.f)) / 2.5f;
    }

    HWY_FAST_MATH_INLINE float Iec61966Eotf(float gamma) {
        if (gamma <= -4.5f * 0.018053968510807f) {
            return std::powf((-gamma + 0.09929682680944f) / -1.09929682680944f, 1.f / 0.45f);
        } else if (gamma < 4.5f * 0.018053968510807f) {
            return gamma / 4.5f;
        }
        return std::powf((gamma + 0.09929682680944f) / 1.09929682680944f, 1.f / 0.45f);
    }

    template<class D, HWY_IF_F32_D(D), typename T = TFromD<D>, typename V = VFromD<D>>
    HWY_FAST_MATH_INLINE V Iec61966Eotf(D d, V v) {
        // The function is non-bijective so choose the middle of [0, 0.01].
        const auto lowerLimit = Set(d, 4.5f * 0.018053968510807f);
        const auto theLowestLimit = Neg(lowerLimit);
        const auto additionToLinear = Set(d, 0.09929682680944f);
        const auto sPower = Set(d, 1.f / 0.45f);
        const auto defScale = Set(d, 1.f / 4.5f);
        const auto linearDivider = ApproximateReciprocal(Set(d, 1.09929682680944f));

        const auto lowest = coder::HWY_NAMESPACE::Pow(d, Mul(Add(Neg(v), additionToLinear),
                                                             Neg(linearDivider)), sPower);
        const auto intmd = Mul(v, defScale);

        const auto highest = coder::HWY_NAMESPACE::Pow(d,
                                                       Mul(Add(v, additionToLinear), linearDivider),
                                                       sPower);
        const auto lowestMask = v <= theLowestLimit;
        const auto upperMask = v >= lowerLimit;

        auto x = IfThenElse(lowestMask, lowest, v);
        x = IfThenElse(And(v > theLowestLimit, v < lowerLimit), Mul(v, defScale), x);
        x = IfThenElse(v > lowerLimit, highest, x);
        return x;
    }

    HWY_FAST_MATH_INLINE float Iec61966Oetf(float linear) {
        if (linear <= -0.018053968510807f) {
            return -1.09929682680944f * std::powf(-linear, 0.45f) + 0.09929682680944f;
        } else if (linear < 0.018053968510807f) {
            return linear * 4.5f;
        }
        return 1.09929682680944f * std::powf(linear, 0.45f) - 0.09929682680944f;
    }

    HWY_FAST_MATH_INLINE float Bt1361Eotf(float gamma) {
        if (gamma < -0.25f) {
            return -0.25f;
        } else if (gamma < 0.f) {
            return std::powf((gamma - 0.02482420670236f) / -0.27482420670236f, 1.f / 0.45f) /
                   -4.f;
        } else if (gamma < 4.5f * 0.018053968510807f) {
            return gamma / 4.5f;
        } else if (gamma < 1.f) {
            return std::powf((gamma + 0.09929682680944f) / 1.09929682680944f, 1.f / 0.45f);
        }
        return 1.f;
    }

    template<class D, HWY_IF_F32_D(D), typename T = TFromD<D>, typename V = VFromD<D>>
    HWY_FAST_MATH_INLINE V Bt1361Eotf(D d, V v) {
        // The function is non-bijective so choose the middle of [0, 0.01].
        const auto lowestPart = Set(d, -0.25f);
        const auto zeros = Zero(d);
        const auto zeroIntensityMod = Set(d, 0.09929682680944f);
        const auto dividerIntensity = Set(d, 1.09929682680944f);
        const auto dividerZeroIntensity = Set(d, -0.27482420670236f);
        const auto sPower = Set(d, 1.f / 0.45f);

        const auto imdScale = Set(d, 1.f / 4.5f);

        const auto imdThreshold = Set(d, 4.5f * 0.018053968510807f);

        const auto lowest = Div(coder::HWY_NAMESPACE::Pow(d, Div(Sub(v, dividerZeroIntensity),
                                                                 dividerZeroIntensity), sPower),
                                Set(d, -4.f));

        const auto highest = coder::HWY_NAMESPACE::Pow(d, Div(Add(v, zeroIntensityMod),
                                                              zeroIntensityMod), sPower);

        const auto ones = Set(d, 1.f);

        auto x = IfThenElse(v >= ones, ones, v);
        x = IfThenElse(v >= imdThreshold, highest, x);
        x = IfThenElse(And(v < imdThreshold, v >= zeros), Div(v, imdThreshold), x);
        x = IfThenElse(And(v >= lowestPart, v < zeros), lowest, x);
        x = IfThenElse(v < lowestPart, lowestPart, x);
        return x;
    }

    HWY_FAST_MATH_INLINE float Bt1361Oetf(float linear) {
        if (linear < -0.25f) {
            return -0.25f;
        } else if (linear < 0.f) {
            return -0.27482420670236f * std::powf(-4.f * linear, 0.45f) + 0.02482420670236f;
        } else if (linear < 0.018053968510807f) {
            return linear * 4.5f;
        } else if (linear < 1.f) {
            return 1.09929682680944f * std::powf(linear, 0.45f) - 0.09929682680944f;
        }
        return 1.f;
    }

    template<typename D>
    class ToneMapper {
    public:
        using V = Vec<D>;

        virtual void Execute(V &R, V &G, V &B) = 0;

        virtual void Execute(TFromD<D> &r, TFromD<D> &g, TFromD<D> &b) = 0;

        virtual ~ToneMapper() {

        }
    };

    template<typename D>
    class LogarithmicToneMapper : public ToneMapper<D> {
    private:
        using V = Vec<D>;
        D df_;
        TFromD<D> den;
        const TFromD<D> exposure;

    public:
        LogarithmicToneMapper(const TFromD<D> lumaCoefficients[3]) : exposure(1), ToneMapper<D>() {
            std::copy(lumaCoefficients, lumaCoefficients + 3, this->lumaCoefficients);
            this->lumaCoefficients[3] = 0.0f;
            TFromD<D> Lmax = 1;
            den = static_cast<TFromD<D>>(1) / log(static_cast<TFromD<D>>(1 + Lmax * exposure));
        }

        ~LogarithmicToneMapper() override = default;

        HWY_FAST_MATH_INLINE void Execute(V &R, V &G, V &B) override {
            const V mExposure = Set(df_, exposure);
            const V lumaR = Set(df_, lumaCoefficients[0]);
            const V lumaG = Set(df_, lumaCoefficients[1]);
            const V lumaB = Set(df_, lumaCoefficients[1]);
            const V denom = Set(df_, den);

            V rLuma = Mul(R, lumaR);
            V gLuma = Mul(G, lumaG);
            V bLuma = Mul(B, lumaB);

            const V ones = Set(df_, static_cast<TFromD<D>>(1.0f));
            const V zeros = Zero(df_);

            V Lin = Mul(Add(Add(rLuma, gLuma), bLuma), mExposure);
            V Lout = Mul(Lognf(df_, Abs(Add(Lin, ones))), denom);

            Lin = IfThenElse(Lin == zeros, ones, Lin);

            V scales = Div(Lout, Lin);
            R = Mul(R, scales);
            G = Mul(G, scales);
            B = Mul(B, scales);
        }

        HWY_FAST_MATH_INLINE void Execute(TFromD<D> &r, TFromD<D> &g, TFromD<D> &b) override {
            const float Lin =
                    r * lumaCoefficients[0] + g * lumaCoefficients[1] + b * lumaCoefficients[2];
            if (Lin == 0) {
                return;
            }
            const TFromD<D> Lout = std::logf(std::abs(1 + Lin)) * den;
            const auto shScale = Lout / Lin;
            r = r * shScale;
            g = g * shScale;
            b = b * shScale;
        }

    private:
        TFromD<D> lumaCoefficients[4];
    };

    template<typename D>
    class Rec2408PQToneMapper : public ToneMapper<D> {
    private:
        using V = Vec<D>;
        D df_;

    public:
        Rec2408PQToneMapper(const TFromD<D> contentMaxBrightness,
                            const TFromD<D> displayMaxBrightness,
                            const TFromD<D> whitePoint,
                            const TFromD<D> lumaCoefficients[3]) : ToneMapper<D>() {
            this->Ld = contentMaxBrightness / whitePoint;
            this->a = (displayMaxBrightness / whitePoint) / (Ld * Ld);
            this->b = 1.0f / (displayMaxBrightness / whitePoint);
            std::copy(lumaCoefficients, lumaCoefficients + 3, this->lumaCoefficients);
            this->lumaCoefficients[3] = 0.0f;
        }

        ~Rec2408PQToneMapper() override = default;

        HWY_FAST_MATH_INLINE void Execute(V &R, V &G, V &B) override {
            const V lumaR = Set(df_, lumaCoefficients[0]);
            const V lumaG = Set(df_, lumaCoefficients[1]);
            const V lumaB = Set(df_, lumaCoefficients[1]);
            const V aVec = Set(df_, this->a);
            const V bVec = Set(df_, this->b);

            V rLuma = Mul(R, lumaR);
            V gLuma = Mul(G, lumaG);
            V bLuma = Mul(B, lumaB);

            const V ones = Set(df_, static_cast<TFromD<D>>(1.0f));

            V Lin = Add(Add(rLuma, gLuma), bLuma);
            V scales = Div(MulAdd(aVec, Lin, ones),
                           MulAdd(bVec, Lin, ones));
            R = Mul(R, scales);
            G = Mul(G, scales);
            B = Mul(B, scales);
        }

        HWY_FAST_MATH_INLINE void Execute(TFromD<D> &r, TFromD<D> &g, TFromD<D> &b) override {
            const float Lin =
                    r * lumaCoefficients[0] + g * lumaCoefficients[1] + b * lumaCoefficients[2];
            if (Lin == 0) {
                return;
            }
            const TFromD<D> shScale = (1.f + this->a * Lin) / (1.f + this->b * Lin);
            r = r * shScale;
            g = g * shScale;
            b = b * shScale;
        }

    private:
        TFromD<D> Ld;
        TFromD<D> a;
        TFromD<D> b;
        TFromD<D> lumaCoefficients[4];
    };

}
HWY_AFTER_NAMESPACE();
#endif