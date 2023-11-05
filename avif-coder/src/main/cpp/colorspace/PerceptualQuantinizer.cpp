/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 07/09/2023
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

#include "PerceptualQuantinizer.h"
#include <vector>
#include <thread>
#include "imagebits/half.hpp"

using namespace half_float;
using namespace std;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "PerceptualQuantinizer.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace coder {
    namespace HWY_NAMESPACE {

        using hwy::HWY_NAMESPACE::ScalableTag;
        using hwy::HWY_NAMESPACE::TFromD;
        using hwy::HWY_NAMESPACE::LoadInterleaved4;
        using hwy::HWY_NAMESPACE::Vec;
        using hwy::HWY_NAMESPACE::Max;
        using hwy::HWY_NAMESPACE::Min;
        using hwy::HWY_NAMESPACE::Div;
        using hwy::HWY_NAMESPACE::Set;
        using hwy::HWY_NAMESPACE::Add;
        using hwy::HWY_NAMESPACE::Rebind;
        using hwy::HWY_NAMESPACE::RebindToSigned;
        using hwy::HWY_NAMESPACE::RebindToFloat;
        using hwy::HWY_NAMESPACE::RebindToUnsigned;
        using hwy::HWY_NAMESPACE::DemoteTo;
        using hwy::HWY_NAMESPACE::ShiftRight;
        using hwy::HWY_NAMESPACE::ShiftLeft;
        using hwy::HWY_NAMESPACE::FixedTag;
        using hwy::HWY_NAMESPACE::ConvertTo;
        using hwy::HWY_NAMESPACE::ExtractLane;
        using hwy::HWY_NAMESPACE::BitCast;
        using hwy::HWY_NAMESPACE::InsertLane;
        using hwy::HWY_NAMESPACE::PromoteTo;
        using hwy::HWY_NAMESPACE::Sub;
        using hwy::HWY_NAMESPACE::Zero;
        using hwy::HWY_NAMESPACE::StoreInterleaved4;
        using hwy::HWY_NAMESPACE::Mul;
        using hwy::HWY_NAMESPACE::LowerHalf;
        using hwy::HWY_NAMESPACE::UpperHalf;
        using hwy::HWY_NAMESPACE::Round;
        using hwy::HWY_NAMESPACE::PromoteLowerTo;
        using hwy::HWY_NAMESPACE::PromoteUpperTo;
        using hwy::HWY_NAMESPACE::RebindToSigned;
        using hwy::HWY_NAMESPACE::Add;
        using hwy::HWY_NAMESPACE::Combine;
        using hwy::HWY_NAMESPACE::IfThenElse;
        using hwy::float16_t;
        using hwy::float32_t;
        
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
        class Rec2408PQToneMapper {
        private:
            using V = hwy::HWY_NAMESPACE::Vec<D>;
            D df_;

        public:
            Rec2408PQToneMapper(const float contentMaxBrightness,
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

        FixedTag<float32_t, 4> fixedFloatTagPQ;

        const Vec<FixedTag<float32_t, 4>> expTab[8] =
                {
                        Set(fixedFloatTagPQ, 1.f),
                        Set(fixedFloatTagPQ, 0.0416598916054f),
                        Set(fixedFloatTagPQ, 0.500000596046f),
                        Set(fixedFloatTagPQ, 0.0014122662833f),
                        Set(fixedFloatTagPQ, 1.00000011921f),
                        Set(fixedFloatTagPQ, 0.00833693705499f),
                        Set(fixedFloatTagPQ, 0.166665703058f),
                        Set(fixedFloatTagPQ, 0.000195780929062f),
                };

        const Vec<FixedTag<float32_t, 4>> logTab[8] =
                {
                        Set(fixedFloatTagPQ, -2.29561495781f),
                        Set(fixedFloatTagPQ, -2.47071170807f),
                        Set(fixedFloatTagPQ, -5.68692588806f),
                        Set(fixedFloatTagPQ, -0.165253549814f),
                        Set(fixedFloatTagPQ, 5.17591238022f),
                        Set(fixedFloatTagPQ, 0.844007015228f),
                        Set(fixedFloatTagPQ, 4.58445882797f),
                        Set(fixedFloatTagPQ, 0.0141278216615f),
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

        inline Vec<FixedTag<float32_t, 4>> LogPQF32(Vec<FixedTag<float32_t, 4>> x) {
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
        PowPQ(Vec<FixedTag<float32_t, 4>> val, Vec<FixedTag<float32_t, 4>> n) {
            return ExpF32(Mul(n, LogPQF32(val)));
        }

        static const float betaRec2020 = 0.018053968510807f;
        static const float alphaRec2020 = 1.09929682680944f;

        inline float bt2020GammaCorrection(float linear) {
            if (0 <= betaRec2020 && linear < betaRec2020) {
                return 4.5f * linear;
            } else if (betaRec2020 <= linear && linear < 1) {
                return alphaRec2020 * powf(linear, 0.45f) - (alphaRec2020 - 1.0f);
            }
            return linear;
        }

        float ToLinearPQ(float v, const float sdrReferencePoint) {
            float o = v;
            v = max(0.0f, v);
            float m1 = (2610.0f / 4096.0f) / 4.0f;
            float m2 = (2523.0f / 4096.0f) * 128.0f;
            float c1 = 3424.0f / 4096.0f;
            float c2 = (2413.0f / 4096.0f) * 32.0f;
            float c3 = (2392.0f / 4096.0f) * 32.0f;
            float p = pow(v, 1.0f / m2);
            v = pow(max(p - c1, 0.0f) / (c2 - c3 * p), 1.0f / m1);
            v *= 10000.0f / sdrReferencePoint;
            return copysign(v, o);
        }

        template<class D, typename T = Vec<D>>
        inline T dciP3PQGammaCorrection(const D d, T color) {
            return PowPQ(color, Set(d, 1 / 2.6f));
        }

        inline float dciP3PQGammaCorrection(float linear) {
            return powf(linear, 1 / 2.6f);
        }

        inline void TransferROWU16HFloats(uint16_t *data, PQGammaCorrection gammaCorrection,
                                          Rec2408PQToneMapper<FixedTag<float, 4>> *toneMapper) {
            auto r = (float) LoadHalf(data[0]);
            auto g = (float) LoadHalf(data[1]);
            auto b = (float) LoadHalf(data[2]);

            r = ToLinearPQ(r, 203.0f);
            g = ToLinearPQ(g, 203.0f);
            b = ToLinearPQ(b, 203.0f);

            toneMapper->Execute(r, g, b);

            if (gammaCorrection == Rec2020) {
                data[0] = half((float) bt2020GammaCorrection(r)).data_;
                data[1] = half((float) bt2020GammaCorrection(g)).data_;
                data[2] = half((float) bt2020GammaCorrection(b)).data_;
            } else if (gammaCorrection == DCIP3) {
                data[0] = half((float) dciP3PQGammaCorrection(r)).data_;
                data[1] = half((float) dciP3PQGammaCorrection(g)).data_;
                data[2] = half((float) dciP3PQGammaCorrection(b)).data_;
            }
        }

        inline void
        TransferROWU8(uint8_t *data, float maxColors, PQGammaCorrection gammaCorrection,
                      Rec2408PQToneMapper<FixedTag<float, 4>> *toneMapper) {
            auto r = (float) data[0] / (float) maxColors;
            auto g = (float) data[1] / (float) maxColors;
            auto b = (float) data[2] / (float) maxColors;

            r = ToLinearPQ(r, 203.0f);
            g = ToLinearPQ(g, 203.0f);
            b = ToLinearPQ(b, 203.0f);

            toneMapper->Execute(r, g, b);

            if (gammaCorrection == Rec2020) {
                data[0] = (uint8_t) clamp((float) bt2020GammaCorrection(r * maxColors), 0.0f,
                                          maxColors);
                data[1] = (uint8_t) clamp((float) bt2020GammaCorrection(g * maxColors), 0.0f,
                                          maxColors);
                data[2] = (uint8_t) clamp((float) bt2020GammaCorrection(b * maxColors), 0.0f,
                                          maxColors);
            } else if (gammaCorrection == DCIP3) {
                data[0] = (uint8_t) clamp((float) dciP3PQGammaCorrection(r * maxColors), 0.0f,
                                          maxColors);
                data[1] = (uint8_t) clamp((float) dciP3PQGammaCorrection(g * maxColors), 0.0f,
                                          maxColors);
                data[2] = (uint8_t) clamp((float) dciP3PQGammaCorrection(b * maxColors), 0.0f,
                                          maxColors);
            }
        }

        template<class D, typename T = Vec<D>>
        inline T bt2020GammaCorrection(const D d, T color) {
            T bt2020 = Set(d, betaRec2020);
            T alpha2020 = Set(d, alphaRec2020);
            const auto cmp = color < bt2020;
            const auto branch1 = Mul(color, Set(d, 4.5f));
            const auto branch2 =
                    Sub(Mul(alpha2020, PowPQ(color, Set(d, 0.45f))),
                        Sub(alpha2020, Set(d, 1.0f)));
            return IfThenElse(cmp, branch1, branch2);
        }

        template<class D, typename T = Vec<D>>
        inline T ToLinearPQ(const D d, T v) {
            v = Max(Zero(d), v);
            float m1 = (2610.0f / 4096.0f) / 4.0f;
            float m2 = (2523.0f / 4096.0f) * 128.0f;
            T c1 = Set(d, 3424.0f / 4096.0f);
            T c2 = Set(d, (2413.0f / 4096.0f) * 32.0f);
            T c3 = Set(d, (2392.0f / 4096.0f) * 32.0f);
            T p = PowPQ(v, Set(d, 1.0f / m2));
            v = PowPQ(Div(Max(Sub(p, c1), Zero(d)), (Sub(c2, Mul(c3, p)))),
                      Set(d, 1.0f / m1));
            v = Mul(v, Set(d, 10000.0f / 203.0f));
            return v;
        }

        void
        ProcessF16Row(uint16_t *HWY_RESTRICT data, int width, PQGammaCorrection gammaCorrection) {
            const FixedTag<float16_t, 4> df16;
            const FixedTag<float32_t, 4> df32;
            const FixedTag<uint16_t, 4> du16;
            const Rebind<float32_t, decltype(df16)> rebind32;
            const Rebind<float16_t, decltype(df32)> rebind16;

            using VU16 = Vec<decltype(du16)>;
            using VF32 = Vec<decltype(df32)>;

            const float lumaPrimaries[3] = {0.2627f, 0.6780f, 0.0593f};
            Rec2408PQToneMapper<FixedTag<float, 4>> toneMapper(1000, 250.0f, 203.0f, lumaPrimaries);

            auto ptr16 = reinterpret_cast<float16_t *>(data);

            int pixels = 4;

            int x;
            for (x = 0; x + pixels < width; x += pixels) {
                VU16 RURow;
                VU16 GURow;
                VU16 BURow;
                VU16 AURow;
                LoadInterleaved4(du16, reinterpret_cast<uint16_t *>(ptr16), RURow, GURow, BURow,
                                 AURow);
                VF32 r32 = PromoteTo(rebind32, BitCast(df16, RURow));
                VF32 g32 = PromoteTo(rebind32, BitCast(df16, GURow));
                VF32 b32 = PromoteTo(rebind32, BitCast(df16, BURow));

                VF32 pqR = ToLinearPQ(df32, r32);
                VF32 pqG = ToLinearPQ(df32, g32);
                VF32 pqB = ToLinearPQ(df32, b32);

                toneMapper.Execute(pqR, pqG, pqB);

                if (gammaCorrection == Rec2020) {
                    pqR = bt2020GammaCorrection(df32, pqR);
                    pqG = bt2020GammaCorrection(df32, pqG);
                    pqB = bt2020GammaCorrection(df32, pqB);
                } else if (gammaCorrection == DCIP3) {
                    pqR = dciP3PQGammaCorrection(df32, pqR);
                    pqG = dciP3PQGammaCorrection(df32, pqG);
                    pqB = dciP3PQGammaCorrection(df32, pqB);
                }

                VU16 rNew = BitCast(du16, DemoteTo(rebind16, pqR));
                VU16 gNew = BitCast(du16, DemoteTo(rebind16, pqG));
                VU16 bNew = BitCast(du16, DemoteTo(rebind16, pqB));

                StoreInterleaved4(rNew, gNew, bNew, AURow, du16,
                                  reinterpret_cast<uint16_t *>(ptr16));
                ptr16 += 4 * 4;
            }

            for (; x < width; ++x) {
                TransferROWU16HFloats(reinterpret_cast<uint16_t *>(ptr16), gammaCorrection,
                                      &toneMapper);
                ptr16 += 4;
            }
        }

        void TransferU8Row(const PQGammaCorrection &gammaCorrection,
                           Rec2408PQToneMapper<FixedTag<float, 4>> &toneMapper,
                           Vec128<float> &R,
                           Vec128<float> &G,
                           Vec128<float> &B,
                           const Vec128<float32_t> vColors,
                           const Vec128<float32_t> zeros) {
            const FixedTag<float32_t, 4> df32;
            using VF32 = Vec<decltype(df32)>;

            VF32 pqR = ToLinearPQ(df32, R);
            VF32 pqG = ToLinearPQ(df32, G);
            VF32 pqB = ToLinearPQ(df32, B);

            toneMapper.Execute(pqR, pqG, pqB);

            if (gammaCorrection == Rec2020) {
                pqR = bt2020GammaCorrection(df32, pqR);
                pqG = bt2020GammaCorrection(df32, pqG);
                pqB = bt2020GammaCorrection(df32, pqB);
            } else if (gammaCorrection == DCIP3) {
                pqR = dciP3PQGammaCorrection(df32, pqR);
                pqG = dciP3PQGammaCorrection(df32, pqG);
                pqB = dciP3PQGammaCorrection(df32, pqB);
            }

            pqR = Max(Min(Round(Mul(pqR, vColors)), vColors), zeros);
            pqG = Max(Min(Round(Mul(pqG, vColors)), vColors), zeros);
            pqB = Max(Min(Round(Mul(pqB, vColors)), vColors), zeros);

            R = pqR;
            G = pqG;
            B = pqB;
        }

        void ProcessUSRow(uint8_t *HWY_RESTRICT data, int width, float maxColors,
                          PQGammaCorrection gammaCorrection) {
            const FixedTag<float32_t, 4> df32;
            FixedTag<uint8_t, 16> d;
            FixedTag<uint16_t, 8> du16;
            FixedTag<uint8_t, 8> du8x8;
            FixedTag<uint32_t, 4> du32;

            const Rebind<int32_t, decltype(df32)> floatToSigned;
            const Rebind<uint16_t, FixedTag<float32_t, 4>> rebindOrigin;

            const float lumaPrimaries[3] = {0.2627f, 0.6780f, 0.0593f};
            Rec2408PQToneMapper<FixedTag<float, 4>> toneMapper(1000, 250.0f, 203.0f, lumaPrimaries);

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
                TransferROWU8(reinterpret_cast<uint8_t *>(ptr16), maxColors, gammaCorrection,
                              &toneMapper);
                ptr16 += 4;
            }
        }
    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(ProcessUSRow);
    HWY_EXPORT(ProcessF16Row);
    HWY_DLLEXPORT void
    ProcessCPURow(uint8_t *data, int y, bool U16, bool halfFloats, int stride, int width,
                  float maxColors, PQGammaCorrection gammaCorrection) {
        if (U16) {
            auto ptr16 = reinterpret_cast<uint16_t *>(data + y * stride);
            if (halfFloats) {
                HWY_DYNAMIC_DISPATCH(ProcessF16Row)(reinterpret_cast<uint16_t *>(ptr16), width,
                                                    gammaCorrection);
            }
        } else {
            auto ptr16 = reinterpret_cast<uint8_t *>(data + y * stride);
            HWY_DYNAMIC_DISPATCH(ProcessUSRow)(reinterpret_cast<uint8_t *>(ptr16),
                                               width,
                                               (float) maxColors, gammaCorrection);
        }
    }
}

void PerceptualQuantinizer::transfer() {
    auto maxColors = powf(2, (float) this->bitDepth) - 1;
    int threadCount = clamp(min(static_cast<int>(std::thread::hardware_concurrency()),
                                this->width * this->height / (256 * 256)), 1, 12);
    std::vector<std::thread> workers;

    int segmentHeight = this->height / threadCount;

    for (int i = 0; i < threadCount; i++) {
        int start = i * segmentHeight;
        int end = (i + 1) * segmentHeight;
        if (i == threadCount - 1) {
            end = this->height;
        }
        workers.emplace_back([start, end, this, maxColors]() {
            for (int y = start; y < end; ++y) {
                coder::ProcessCPURow(this->rgbaData, y, this->U16, this->halfFloats,
                                     this->stride, this->width, maxColors, this->gammaCorrection);
            }
        });
    }

    for (std::thread &thread: workers) {
        thread.join();
    }
}

#endif
