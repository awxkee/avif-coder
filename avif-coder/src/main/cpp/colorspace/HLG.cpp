/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 15/9/2023
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

#include "HLG.h"
#include <vector>
#include <thread>
#include "imagebits/half.hpp"

using namespace std;
using namespace half_float;

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

        using hwy::HWY_NAMESPACE::Mul;
        using hwy::HWY_NAMESPACE::Vec128;
        using hwy::HWY_NAMESPACE::FixedTag;
        using hwy::HWY_NAMESPACE::Set;
        using hwy::HWY_NAMESPACE::ReduceSum;
        using hwy::HWY_NAMESPACE::GetLane;
        using hwy::HWY_NAMESPACE::Load;
        using hwy::HWY_NAMESPACE::Add;
        using hwy::HWY_NAMESPACE::Div;
        using hwy::HWY_NAMESPACE::MulAdd;

        template<typename D>
        class Rec2408HLGToneMapper {
        private:
            using V = hwy::HWY_NAMESPACE::Vec<D>;
            D df_;

        public:
            Rec2408HLGToneMapper(const float contentMaxBrightness,
                                 const float displayMaxBrightness,
                                 const float whitePoint,
                                 const float lumaCoefficients[3]) {
                this->Ld = contentMaxBrightness / whitePoint;
                this->a = (displayMaxBrightness / whitePoint) / (Ld * Ld);
                this->b = 1.0f / (displayMaxBrightness / whitePoint);
                std::copy(lumaCoefficients, lumaCoefficients + 3, this->lumaCoefficients);
                this->lumaCoefficients[3] = 0.0f;
            }

            inline void Execute(V &R, V &G, V &B) {

                V rLuma = Mul(R, Set(df_, lumaCoefficients[0]));
                V gLuma = Mul(G, Set(df_, lumaCoefficients[1]));
                V bLuma = Mul(B, Set(df_, lumaCoefficients[2]));

                V Lin = Add(Add(rLuma, gLuma), bLuma);
                V scales = Div(MulAdd(Set(df_, this->a), Lin, Set(df_, 1.0f)),
                               MulAdd(Set(df_, this->b), Lin, Set(df_, 1.0f)));
                R = Mul(R, scales);
                G = Mul(G, scales);
                B = Mul(B, scales);
            }

            inline void Execute(float &r, float &g, float &b) {
                const float Lin =
                        r * lumaCoefficients[0] + g * lumaCoefficients[1] + b * lumaCoefficients[2];
                if (Lin == 0) {
                    return;
                }
                const float shScale = (1.f + this->a * Lin) / (1.f + this->b * Lin);
                r = r * shScale;
                g = g * shScale;
                b = b * shScale;
            }

        private:
            float Ld;
            float a;
            float b;
            float lumaCoefficients[4];
        };

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

        template<class D, typename T = Vec<D>>
        inline T dciP3GammaCorrection(const D d, T color) {
            return PowHLG(color, Set(d, 1 / 2.6f));
        }

        inline float dciP3HLGGammaCorrection(float linear) {
            return powf(linear, 1 / 2.6f);
        }

        inline float bt2020HLGGammaCorrection(float linear) {
            static const float betaRec2020 = 0.018053968510807f;
            static const float alphaRec2020 = 1.09929682680944f;
            if (0 <= betaRec2020 && linear < betaRec2020) {
                return 4.5f * linear;
            } else if (betaRec2020 <= linear && linear < 1) {
                return alphaRec2020 * powf(linear, 0.45f) - (alphaRec2020 - 1.0f);
            }
            return linear;
        }

        inline void
        TransferROWHLGU8(uint8_t *data, float maxColors, HLGGammaCorrection gammaCorrection,
                         Rec2408HLGToneMapper<FixedTag<float, 4>> *toneMapper) {
            auto r = (float) data[0] / (float) maxColors;
            auto g = (float) data[1] / (float) maxColors;
            auto b = (float) data[2] / (float) maxColors;

            r = Evaluate(r);
            g = Evaluate(g);
            b = Evaluate(b);

            toneMapper->Execute(r, g, b);

            if (gammaCorrection == Rec2020) {
                data[0] = (uint8_t) clamp(
                        (float) bt2020HLGGammaCorrection(round(r)) * maxColors, 0.0f, maxColors);
                data[1] = (uint8_t) clamp(
                        (float) bt2020HLGGammaCorrection(round(g)) * maxColors, 0.0f, maxColors);
                data[2] = (uint8_t) clamp(
                        (float) bt2020HLGGammaCorrection(round(b)) * maxColors, 0.0f, maxColors);
            } else if (gammaCorrection == DCIP3) {
                data[0] = (uint8_t) clamp(
                        (float) dciP3HLGGammaCorrection(round(r)) * maxColors, 0.0f, maxColors);
                data[1] = (uint8_t) clamp(
                        (float) dciP3HLGGammaCorrection(round(g)) * maxColors, 0.0f, maxColors);
                data[2] = (uint8_t) clamp(
                        (float) dciP3HLGGammaCorrection(round(b)) * maxColors, 0.0f, maxColors);
            }
        }

        inline void
        TransferROWHLGU16(uint16_t *data, float maxColors, HLGGammaCorrection gammaCorrection,
                          Rec2408HLGToneMapper<FixedTag<float, 4>> *toneMapper) {
            auto r = (float) data[0] / (float) maxColors;
            auto g = (float) data[1] / (float) maxColors;
            auto b = (float) data[2] / (float) maxColors;

            r = Evaluate(r);
            g = Evaluate(g);
            b = Evaluate(b);

            toneMapper->Execute(r, g, b);

            if (gammaCorrection == Rec2020) {
                data[0] = (uint16_t) half((float) bt2020HLGGammaCorrection(round(r))).data_;
                data[1] = (uint16_t) half((float) bt2020HLGGammaCorrection(round(g))).data_;
                data[2] = (uint16_t) half((float) bt2020HLGGammaCorrection(round(b))).data_;
            } else if (gammaCorrection == DCIP3) {
                data[0] = (uint16_t) half((float) dciP3HLGGammaCorrection(round(r))).data_;
                data[1] = (uint16_t) half((float) dciP3HLGGammaCorrection(round(g))).data_;
                data[2] = (uint16_t) half((float) dciP3HLGGammaCorrection(round(b))).data_;
            }
        }

        void ProcessHLGF16Row(uint16_t *HWY_RESTRICT data, int width, float maxColors,
                              HLGGammaCorrection gammaCorrection) {
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

            const float lumaPrimaries[3] = {0.2627f, 0.6780f, 0.0593f};
            Rec2408HLGToneMapper<FixedTag<float, 4>> toneMapper(1000, 250.0f, 203.0f,
                                                                lumaPrimaries);

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

                toneMapper.Execute(r32, g32, b32);

                if (gammaCorrection == Rec2020) {
                    r32 = bt2020HLGGammaCorrection(df32, HLGEotf(r32));
                    g32 = bt2020HLGGammaCorrection(df32, HLGEotf(g32));
                    b32 = bt2020HLGGammaCorrection(df32, HLGEotf(b32));
                } else if (gammaCorrection == DCIP3) {
                    r32 = dciP3GammaCorrection(df32, HLGEotf(r32));
                    g32 = dciP3GammaCorrection(df32, HLGEotf(g32));
                    b32 = dciP3GammaCorrection(df32, HLGEotf(b32));
                }

                VF32 pqR = r32;
                VF32 pqG = g32;
                VF32 pqB = b32;

                VU16 rNew = BitCast(du16, DemoteTo(rebind16, pqR));
                VU16 gNew = BitCast(du16, DemoteTo(rebind16, pqG));
                VU16 bNew = BitCast(du16, DemoteTo(rebind16, pqB));

                StoreInterleaved4(rNew, gNew, bNew, AURow, d,
                                  reinterpret_cast<uint16_t *>(ptr16));
                ptr16 += 4 * 4;
            }

            for (; x < width; ++x) {
                TransferROWHLGU16(reinterpret_cast<uint16_t *>(ptr16), maxColors, gammaCorrection,
                                  &toneMapper);
                ptr16 += 4;
            }
        }

        void TransferU8Row(const HLGGammaCorrection &gammaCorrection,
                           Rec2408HLGToneMapper<FixedTag<float, 4>> &toneMapper,
                           Vec128<float> &R,
                           Vec128<float> &G,
                           Vec128<float> &B,
                           const Vec128<float32_t> vColors,
                           const Vec128<float32_t> zeros) {
            const FixedTag<float32_t, 4> df32;
            using VF32 = Vec<decltype(df32)>;

            VF32 pqR = HLGEotf(R);
            VF32 pqG = HLGEotf(G);
            VF32 pqB = HLGEotf(B);

            toneMapper.Execute(pqR, pqG, pqB);

            if (gammaCorrection == Rec2020) {
                pqR = bt2020HLGGammaCorrection(df32, pqR);
                pqG = bt2020HLGGammaCorrection(df32, pqG);
                pqB = bt2020HLGGammaCorrection(df32, pqB);
            } else if (gammaCorrection == DCIP3) {
                pqR = dciP3GammaCorrection(df32, pqR);
                pqG = dciP3GammaCorrection(df32, pqG);
                pqB = dciP3GammaCorrection(df32, pqB);
            }

            pqR = Max(Min(Round(Mul(pqR, vColors)), vColors), zeros);
            pqG = Max(Min(Round(Mul(pqG, vColors)), vColors), zeros);
            pqB = Max(Min(Round(Mul(pqB, vColors)), vColors), zeros);

            R = pqR;
            G = pqG;
            B = pqB;
        }

        void ProcessHLGu8Row(uint8_t *HWY_RESTRICT data, int width, float maxColors,
                             HLGGammaCorrection gammaCorrection) {
            const FixedTag<float32_t, 4> df32;
            FixedTag<uint8_t, 16> d;
            FixedTag<uint16_t, 8> du16;
            FixedTag<uint8_t, 8> du8x8;
            FixedTag<uint32_t, 4> du32;

            const Rebind<int32_t, decltype(df32)> floatToSigned;
            const Rebind<uint16_t, FixedTag<float32_t, 4>> rebindOrigin;

            const float lumaPrimaries[3] = {0.2627f, 0.6780f, 0.0593f};
            Rec2408HLGToneMapper<FixedTag<float, 4>> toneMapper(1000, 250.0f, 203.0f,
                                                                lumaPrimaries);

            const Rebind<float32_t, decltype(du32)> rebind32;

            using VU8 = Vec<decltype(d)>;
            using VF32 = Vec<decltype(df32)>;

            auto ptr16 = reinterpret_cast<uint8_t *>(data);

            VF32 vColors = Set(df32, (float) maxColors);
            const VF32 vZeros = Zero(df32);

            int pixels = 4 * 4;

            int x = 0;
            for (x = 0; x + pixels < width; x += pixels) {
                VU8 RURow;
                VU8 GURow;
                VU8 BURow;
                VU8 AURow;
                LoadInterleaved4(d, reinterpret_cast<uint8_t *>(ptr16), RURow, GURow, BURow, AURow);
                auto lowR16 = PromoteLowerTo(du16, RURow);
                auto lowG16 = PromoteLowerTo(du16, GURow);
                auto lowB16 = PromoteLowerTo(du16, BURow);

                VF32 rLowerLow32 = Div(ConvertTo(rebind32, PromoteLowerTo(du32, lowR16)), vColors);
                VF32 gLowerLow32 = Div(ConvertTo(rebind32, PromoteLowerTo(du32, lowG16)), vColors);
                VF32 bLowerLow32 = Div(ConvertTo(rebind32, PromoteLowerTo(du32, lowB16)), vColors);

                TransferU8Row(gammaCorrection, toneMapper, rLowerLow32, gLowerLow32, bLowerLow32,
                              vColors, vZeros);

                VF32 rLowerHigh32 = Div(ConvertTo(rebind32, PromoteUpperTo(du32, lowR16)), vColors);
                VF32 gLowerHigh32 = Div(ConvertTo(rebind32, PromoteUpperTo(du32, lowG16)), vColors);
                VF32 bLowerHigh32 = Div(ConvertTo(rebind32, PromoteUpperTo(du32, lowB16)), vColors);

                TransferU8Row(gammaCorrection, toneMapper, rLowerHigh32, gLowerHigh32, bLowerHigh32,
                              vColors, vZeros);

                auto upperR16 = PromoteUpperTo(du16, RURow);
                auto upperG16 = PromoteUpperTo(du16, GURow);
                auto upperB16 = PromoteUpperTo(du16, BURow);

                VF32 rHigherLow32 = Div(ConvertTo(rebind32, PromoteLowerTo(du32, upperR16)),
                                        vColors);
                VF32 gHigherLow32 = Div(ConvertTo(rebind32, PromoteLowerTo(du32, upperG16)),
                                        vColors);
                VF32 bHigherLow32 = Div(ConvertTo(rebind32, PromoteLowerTo(du32, lowB16)), vColors);

                TransferU8Row(gammaCorrection, toneMapper, rHigherLow32, gHigherLow32, bHigherLow32,
                              vColors, vZeros);

                VF32 rHigherHigh32 = Div(ConvertTo(rebind32, PromoteUpperTo(du32, upperR16)),
                                         vColors);
                VF32 gHigherHigh32 = Div(ConvertTo(rebind32, PromoteUpperTo(du32, upperG16)),
                                         vColors);
                VF32 bHigherHigh32 = Div(ConvertTo(rebind32, PromoteUpperTo(du32, upperB16)),
                                         vColors);

                TransferU8Row(gammaCorrection, toneMapper, rHigherHigh32, gHigherHigh32,
                              bHigherHigh32, vColors, vZeros);

                auto rNew = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, rHigherHigh32));
                auto gNew = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, gHigherHigh32));
                auto bNew = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, bHigherHigh32));

                auto r1New = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, rHigherLow32));
                auto g1New = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, gHigherLow32));
                auto b1New = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, bHigherLow32));

                auto highR = Combine(du16, rNew, r1New);
                auto highG = Combine(du16, gNew, g1New);
                auto highB = Combine(du16, bNew, b1New);

                rNew = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, rLowerHigh32));
                gNew = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, gLowerHigh32));
                bNew = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, bLowerHigh32));

                r1New = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, rLowerLow32));
                g1New = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, gLowerLow32));
                b1New = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, bLowerLow32));

                auto lowR = Combine(du16, rNew, r1New);
                auto lowG = Combine(du16, gNew, g1New);
                auto lowB = Combine(du16, bNew, b1New);

                auto rU8x16 = Combine(d, DemoteTo(du8x8, highR), DemoteTo(du8x8, lowR));
                auto gU8x16 = Combine(d, DemoteTo(du8x8, highG), DemoteTo(du8x8, lowG));
                auto bU8x16 = Combine(d, DemoteTo(du8x8, highB), DemoteTo(du8x8, lowB));

                StoreInterleaved4(rU8x16, gU8x16, bU8x16, AURow, d,
                                  reinterpret_cast<uint8_t *>(ptr16));
                ptr16 += 4 * 16;
            }

            for (; x < width; ++x) {
                TransferROWHLGU8(reinterpret_cast<uint8_t *>(ptr16), maxColors, gammaCorrection,
                                 &toneMapper);
                ptr16 += 4;
            }
        }

        void
        ProcessHLG(uint8_t *data, bool halfFloats, int stride, int width, int height, int depth,
                   HLGGammaCorrection correction) {
            float maxColors = powf(2, (float) depth) - 1;
            int threadCount = clamp(min(static_cast<int>(std::thread::hardware_concurrency()),
                                        height * width / (256 * 256)), 1, 12);
            std::vector<std::thread> workers;

            int segmentHeight = height / threadCount;

            for (int i = 0; i < threadCount; i++) {
                int start = i * segmentHeight;
                int end = (i + 1) * segmentHeight;
                if (i == threadCount - 1) {
                    end = height;
                }
                workers.emplace_back(
                        [start, end, halfFloats, maxColors, data, stride, width, correction]() {
                            for (int y = start; y < end; ++y) {
                                if (halfFloats) {
                                    auto ptr16 = reinterpret_cast<uint16_t *>(data + y * stride);
                                    ProcessHLGF16Row(reinterpret_cast<uint16_t *>(ptr16),
                                                     width,
                                                     (float) maxColors, correction);
                                } else {
                                    auto ptr16 = reinterpret_cast<uint8_t *>(data + y * stride);
                                    ProcessHLGu8Row(reinterpret_cast<uint8_t *>(ptr16),
                                                    width,
                                                    (float) maxColors, correction);
                                }
                            }
                        });
            }

            for (std::thread &thread: workers) {
                thread.join();
            }
        }

    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(ProcessHLG);
    HWY_DLLEXPORT void
    ProcessHLG(uint8_t *data, bool halfFloats, int stride, int width, int height, int depth,
               HLGGammaCorrection gammaCorrection) {
        HWY_DYNAMIC_DISPATCH(ProcessHLG)(data, halfFloats, stride, width, height, depth,
                                         gammaCorrection);
    }
}
#endif