//
// Created by Radzivon Bartoshyk on 15/09/2023.
//

#include "HLG.h"
#include "ThreadPool.hpp"
#include "HalfFloats.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "HLG.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace coder {
    namespace HWY_NAMESPACE {

        using hwy::HWY_NAMESPACE::Set;
        using hwy::HWY_NAMESPACE::FixedTag;
        using hwy::HWY_NAMESPACE::Vec;
        using hwy::HWY_NAMESPACE::Add;
        using hwy::HWY_NAMESPACE::Sub;
        using hwy::HWY_NAMESPACE::Div;
        using hwy::HWY_NAMESPACE::Min;
        using hwy::HWY_NAMESPACE::Max;
        using hwy::HWY_NAMESPACE::Mul;
        using hwy::HWY_NAMESPACE::MulAdd;
        using hwy::HWY_NAMESPACE::MulSub;
        using hwy::HWY_NAMESPACE::NegMulAdd;
        using hwy::HWY_NAMESPACE::Rebind;
        using hwy::HWY_NAMESPACE::ConvertTo;
        using hwy::HWY_NAMESPACE::DemoteTo;
        using hwy::HWY_NAMESPACE::BitCast;
        using hwy::HWY_NAMESPACE::ShiftLeft;
        using hwy::HWY_NAMESPACE::ShiftRight;
        using hwy::float32_t;
        using hwy::float16_t;

        FixedTag<float32_t, 4> fixedFloatTag;

        const Vec<FixedTag<float32_t, 4>> expTab[8] =
                {
                        Set(fixedFloatTag, 1.f),
                        Set(fixedFloatTag, 0.0416598916054f),
                        Set(fixedFloatTag, 0.500000596046f),
                        Set(fixedFloatTag, 0.0014122662833f),
                        Set(fixedFloatTag, 1.00000011921f),
                        Set(fixedFloatTag, 0.00833693705499f),
                        Set(fixedFloatTag, 0.166665703058f),
                        Set(fixedFloatTag, 0.000195780929062f),
                };

        const Vec<FixedTag<float32_t, 4>> logTab[8] =
                {
                        Set(fixedFloatTag, -2.29561495781f),
                        Set(fixedFloatTag, -2.47071170807f),
                        Set(fixedFloatTag, -5.68692588806f),
                        Set(fixedFloatTag, -0.165253549814f),
                        Set(fixedFloatTag, 5.17591238022f),
                        Set(fixedFloatTag, 0.844007015228f),
                        Set(fixedFloatTag, 4.58445882797f),
                        Set(fixedFloatTag, 0.0141278216615f),
                };

        inline Vec<FixedTag<float32_t, 4>>
        TaylorPolyExp(Vec<FixedTag<float32_t, 4>> x, const Vec<FixedTag<float32_t, 4>> *coeffs) {
            FixedTag<float32_t, 4> d;
            using VF32 = Vec<decltype(d)>;

            VF32 A = MulAdd(coeffs[4], x, coeffs[0]);
            VF32 B = MulAdd(coeffs[6], x, coeffs[2]);
            VF32 C = MulAdd(coeffs[5], x, coeffs[1]);
            VF32 D = MulAdd(coeffs[7], x, coeffs[3]);
            VF32 x2 = Mul(x, x);
            VF32 x4 = Mul(x2, x2);
            VF32 res = MulAdd(MulAdd(D, x2, C), x4, MulAdd(B, x2, A));
            return res;
        }

        inline Vec<FixedTag<float32_t, 4>> ExpF32(Vec<FixedTag<float32_t, 4>> x) {
            FixedTag<float32_t, 4> d;
            using VF32 = Vec<decltype(d)>;
            Rebind<int32_t, decltype(d)> s32;
            using VS32 = Vec<decltype(s32)>;
            Rebind<float32_t, decltype(s32)> fs32;
            static const VF32 CONST_LN2 = Set(d, 0.6931471805f); // ln(2)
            static const VF32 CONST_INV_LN2 = Set(d, 1.4426950408f); // 1/ln(2)
            // Perform range reduction [-log(2),log(2)]
            VS32 m = ConvertTo(s32, Mul(x, CONST_INV_LN2));
            auto val = NegMulAdd(ConvertTo(fs32, m), CONST_LN2, x);
            // Polynomial Approximation
            auto poly = TaylorPolyExp(val, &expTab[0]);
            // Reconstruct
            poly = BitCast(fs32, Add(BitCast(s32, poly), ShiftLeft<23>(m)));
            return poly;
        }

        inline Vec<FixedTag<float32_t, 4>> LogF32(Vec<FixedTag<float32_t, 4>> x) {
            FixedTag<float32_t, 4> d;
            using VF32 = Vec<decltype(d)>;
            FixedTag<int32_t, 4> s32;
            Rebind<int32_t, decltype(d)> sb32;
            Rebind<float32_t, decltype(s32)> fli32;
            using VS32 = Vec<decltype(s32)>;
            static const VS32 CONST_127 = Set(s32, 127);           // 127
            static const VF32 CONST_LN2 = Set(d, 0.6931471805f); // ln(2)
            // Extract exponent
            VS32 m = Sub(BitCast(s32, ShiftRight<23>(BitCast(s32, BitCast(sb32, x)))), CONST_127);
            VF32 val = BitCast(fli32,
                               Sub(BitCast(sb32, x), ShiftLeft<23>(m)));
            // Polynomial Approximation
            VF32 poly = TaylorPolyExp(val, &logTab[0]);
            // Reconstruct
            poly = MulAdd(ConvertTo(fli32, m), CONST_LN2, poly);
            return poly;
        }

        inline Vec<FixedTag<float32_t, 4>>
        PowHLG(Vec<FixedTag<float32_t, 4>> val, Vec<FixedTag<float32_t, 4>> n) {
            return ExpF32(Mul(n, LogF32(val)));
        }

        inline Vec<FixedTag<float32_t, 4>> HLGEotf(Vec<FixedTag<float32_t, 4>> v) {
            v = Max(Zero(fixedFloatTag), v);
            using VF32 = Vec<decltype(fixedFloatTag)>;
            VF32 a = Set(fixedFloatTag, 0.17883277f);
            VF32 b = Set(fixedFloatTag, 0.28466892f);
            VF32 c = Set(fixedFloatTag, 0.55991073f);
            VF32 mm = Set(fixedFloatTag, 0.5f);
            const auto cmp = v < mm;
            auto branch1 = Div(Mul(v, v), Set(fixedFloatTag, 3.f));
            auto branch2 = Div(ExpF32(Add(Div(Sub(v, c), a), b)), Set(fixedFloatTag, 12.0f));
            return IfThenElse(cmp, branch1, branch2);
        }

        float Evaluate(float v) {
            // Spec: http://www.arib.or.jp/english/html/overview/doc/2-STD-B67v1_0.pdf
            v = std::max(0.0f, v);
            constexpr float a = 0.17883277f;
            constexpr float b = 0.28466892f;
            constexpr float c = 0.55991073f;
            if (v <= 0.5f)
                v = v * v / 3.0f;
            else
                v = (exp((v - c) / a) + b) / 12.f;
            return v;
        }

        template<class D, typename T = Vec<D>>
        inline T bt2020HLGGammaCorrection(const D d, T color) {
            static const float betaRec2020 = 0.018053968510807f;
            static const float alphaRec2020 = 1.09929682680944f;
            T bt2020 = Set(d, betaRec2020);
            T alpha2020 = Set(d, alphaRec2020);
            const auto cmp = color < bt2020;
            const auto branch1 = Mul(color, Set(d, 4.5f));
            const auto branch2 =
                    Sub(Mul(alpha2020, PowHLG(color, Set(d, 0.45f))), Sub(alpha2020, Set(d, 1.0f)));
            return IfThenElse(cmp, branch1, branch2);
        }

        float bt2020HLGGammaCorrection(float linear) {
            static const float betaRec2020 = 0.018053968510807f;
            static const float alphaRec2020 = 1.09929682680944f;
            if (0 <= betaRec2020 && linear < betaRec2020) {
                return 4.5f * linear;
            } else if (betaRec2020 <= linear && linear < 1) {
                return alphaRec2020 * powf(linear, 0.45f) - (alphaRec2020 - 1.0f);
            }
            return linear;
        }

        void TransferROWHLGU8(uint8_t *data, float maxColors) {
            auto r = (float) data[0] / (float) maxColors;
            auto g = (float) data[1] / (float) maxColors;
            auto b = (float) data[2] / (float) maxColors;
            data[0] = (uint8_t) std::clamp(
                    (float) bt2020HLGGammaCorrection(Evaluate(r)) * maxColors, 0.0f, maxColors);
            data[1] = (uint8_t) std::clamp(
                    (float) bt2020HLGGammaCorrection(Evaluate(g)) * maxColors, 0.0f, maxColors);
            data[2] = (uint8_t) std::clamp(
                    (float) bt2020HLGGammaCorrection(Evaluate(b)) * maxColors, 0.0f, maxColors);
        }

        void TransferROWHLGU16(uint16_t *data, float maxColors) {
            auto r = (float) data[0] / (float) maxColors;
            auto g = (float) data[1] / (float) maxColors;
            auto b = (float) data[2] / (float) maxColors;
            data[0] = (uint16_t) float_to_half((float) bt2020HLGGammaCorrection(Evaluate(r)));
            data[1] = (uint16_t) float_to_half((float) bt2020HLGGammaCorrection(Evaluate(g)));
            data[2] = (uint16_t) float_to_half((float) bt2020HLGGammaCorrection(Evaluate(b)));
        }

        void ProcessHLGF16Row(uint16_t *HWY_RESTRICT data, int width, float maxColors) {
            const FixedTag<float32_t, 4> df32;
            FixedTag<uint16_t, 4> d;

            const Rebind<uint32_t, decltype(d)> signed32;

            const Rebind<float32_t, decltype(signed32)> rebind32;
            const FixedTag<uint16_t, 4> du16;
            const Rebind<float16_t, decltype(df32)> rebind16;

            using VU16 = Vec<decltype(d)>;
            using VF32 = Vec<decltype(df32)>;

            auto ptr16 = reinterpret_cast<uint16_t *>(data);

            int pixels = 4;

            int x = 0;
            for (x = 0; x + pixels < width; x += pixels) {
                VU16 RURow;
                VU16 GURow;
                VU16 BURow;
                VU16 AURow;
                LoadInterleaved4(d, reinterpret_cast<uint16_t *>(ptr16), RURow, GURow, BURow,
                                 AURow);
                VF32 r32 = BitCast(rebind32, PromoteTo(signed32, RURow));
                VF32 g32 = BitCast(rebind32, PromoteTo(signed32, GURow));
                VF32 b32 = BitCast(rebind32, PromoteTo(signed32, BURow));

                VF32 pqR = bt2020HLGGammaCorrection(df32, HLGEotf(r32));
                VF32 pqG = bt2020HLGGammaCorrection(df32, HLGEotf(g32));
                VF32 pqB = bt2020HLGGammaCorrection(df32, HLGEotf(b32));

                VU16 rNew = BitCast(du16, DemoteTo(rebind16, pqR));
                VU16 gNew = BitCast(du16, DemoteTo(rebind16, pqG));
                VU16 bNew = BitCast(du16, DemoteTo(rebind16, pqB));

                StoreInterleaved4(rNew, gNew, bNew, AURow, d,
                                  reinterpret_cast<uint16_t *>(ptr16));
                ptr16 += 4 * 4;
            }

            for (; x < width; ++x) {
                TransferROWHLGU16(reinterpret_cast<uint16_t *>(ptr16), maxColors);
                ptr16 += 4;
            }
        }

        void ProcessHLGu8Row(uint8_t *HWY_RESTRICT data, int width, float maxColors) {
            const FixedTag<float32_t, 4> df32;
            hwy::HWY_NAMESPACE::FixedTag<uint8_t, 4> d;

            const Rebind<uint32_t, decltype(d)> signed32;
            const Rebind<int32_t, decltype(df32)> floatToSigned;
            const Rebind<uint8_t, FixedTag<float32_t, 4>> rebindOrigin;

            const Rebind<float32_t, decltype(signed32)> rebind32;

            using VU16 = Vec<decltype(d)>;
            using VF32 = Vec<decltype(df32)>;

            auto ptr16 = reinterpret_cast<uint8_t *>(data);

            VF32 vColors = Set(df32, (float) maxColors);

            int pixels = 4;

            int x = 0;
            for (x = 0; x + pixels < width; x += pixels) {
                VU16 RURow;
                VU16 GURow;
                VU16 BURow;
                VU16 AURow;
                LoadInterleaved4(d, reinterpret_cast<uint8_t *>(ptr16), RURow, GURow, BURow, AURow);
                VF32 r32 = Div(ConvertTo(rebind32, PromoteTo(signed32, RURow)), vColors);
                VF32 g32 = Div(ConvertTo(rebind32, PromoteTo(signed32, GURow)), vColors);
                VF32 b32 = Div(ConvertTo(rebind32, PromoteTo(signed32, BURow)), vColors);

                VF32 pqR = Max(
                        Min(Mul(bt2020HLGGammaCorrection(df32, HLGEotf(r32)), vColors), vColors),
                        Zero(df32));
                VF32 pqG = Max(
                        Min(Mul(bt2020HLGGammaCorrection(df32, HLGEotf(g32)), vColors), vColors),
                        Zero(df32));
                VF32 pqB = Max(
                        Min(Mul(bt2020HLGGammaCorrection(df32, HLGEotf(b32)), vColors), vColors),
                        Zero(df32));

                VU16 rNew = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, pqR));
                VU16 gNew = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, pqG));
                VU16 bNew = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, pqB));

                StoreInterleaved4(rNew, gNew, bNew, AURow, d,
                                  reinterpret_cast<uint8_t *>(ptr16));
                ptr16 += 4 * 4;
            }

            for (; x < width; ++x) {
                TransferROWHLGU8(reinterpret_cast<uint8_t *>(ptr16), maxColors);
                ptr16 += 4;
            }
        }

        void
        ProcessHLG(uint8_t *data, bool halfFloats, int stride, int width, int height, int depth) {
            float maxColors = powf(2, (float) depth) - 1;
            ThreadPool pool;
            std::vector<std::future<void>> results;
            for (int y = 0; y < height; ++y) {
                if (halfFloats) {
                    auto ptr16 = reinterpret_cast<uint16_t *>(data + y * stride);
                    auto r = pool.enqueue(ProcessHLGF16Row, reinterpret_cast<uint16_t *>(ptr16),
                                          width,
                                          (float) maxColors);
                    results.push_back(std::move(r));
                } else {
                    auto ptr16 = reinterpret_cast<uint8_t *>(data + y * stride);
                    auto r = pool.enqueue(ProcessHLGu8Row, reinterpret_cast<uint8_t *>(ptr16),
                                          width,
                                          (float) maxColors);
                    results.push_back(std::move(r));
                }
            }

            for (auto &result: results) {
                result.wait();
            }
        }

    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(ProcessHLG);
    HWY_DLLEXPORT void
    ProcessHLG(uint8_t *data, bool halfFloats, int stride, int width, int height, int depth) {
        HWY_DYNAMIC_DISPATCH(ProcessHLG)(data, halfFloats, stride, width, height, depth);
    }
}
#endif