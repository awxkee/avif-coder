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
#include "ThreadPool.hpp"
#include "imagebits/half.hpp"

using namespace half_float;

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
        using hwy::HWY_NAMESPACE::RebindToSigned;
        using hwy::HWY_NAMESPACE::Add;
        using hwy::HWY_NAMESPACE::IfThenElse;
        using hwy::float16_t;
        using hwy::float32_t;

        const float expTab[8] =
                {
                        1.f,
                        0.0416598916054f,
                        0.500000596046f,
                        0.0014122662833f,
                        1.00000011921f,
                        0.00833693705499f,
                        0.166665703058f,
                        0.000195780929062f,
                };

        const float logTab[8] =
                {
                        -2.29561495781f,
                        -2.47071170807f,
                        -5.68692588806f,
                        -0.165253549814f,
                        5.17591238022f,
                        0.844007015228f,
                        4.58445882797f,
                        0.0141278216615f,
                };

        template<typename T, size_t N, HWY_IF_FLOAT(T)>
        inline Vec<FixedTag<T, N>>
        TaylorPolyExp(Vec<FixedTag<T, N>> x, const float *coeffs) {
            FixedTag<T, N> d;
            using VF32 = Vec<decltype(d)>;

            VF32 A = MulAdd(Set(d, coeffs[4]), x, Set(d, coeffs[0]));
            VF32 B = MulAdd(Set(d, coeffs[6]), x, Set(d, coeffs[2]));
            VF32 C = MulAdd(Set(d, coeffs[5]), x, Set(d, coeffs[1]));
            VF32 D = MulAdd(Set(d, coeffs[1]), x, Set(d, coeffs[3]));
            VF32 x2 = Mul(x, x);
            VF32 x4 = Mul(x2, x2);
            VF32 res = MulAdd(MulAdd(D, x2, C), x4, MulAdd(B, x2, A));
            return res;
        }

        template<typename T, size_t N, HWY_IF_FLOAT(T)>
        inline Vec<FixedTag<T, N>> ExpF32(Vec<FixedTag<T, N>> x) {
            FixedTag<T, N> d;
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
            auto poly = TaylorPolyExp<float32_t, 4>(val, &expTab[0]);
            // Reconstruct
            poly = BitCast(fs32, Add(BitCast(s32, poly), ShiftLeft<23>(m)));
            return poly;
        }

        template<typename T, size_t N, HWY_IF_FLOAT(T)>
        inline Vec<FixedTag<T, N>> LogPQF32(Vec<FixedTag<T, N>> x) {
            FixedTag<T, N> d;
            using VF32 = Vec<decltype(d)>;
            FixedTag<int32_t, N> s32;
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
            VF32 poly = TaylorPolyExp<float32_t, 4>(val, &logTab[0]);
            // Reconstruct
            poly = MulAdd(ConvertTo(fli32, m), CONST_LN2, poly);
            return poly;
        }

        template<typename T, size_t N, HWY_IF_FLOAT(T)>
        inline Vec<FixedTag<T, N>>
        PowPQ(Vec<FixedTag<T, N>> val, Vec<FixedTag<T, N>> n) {
            return ExpF32<T, N>(Mul(n, LogPQF32<T, N>(val)));
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

        inline float clamp(float value, float min, float max) {
            return std::min(std::max(value, min), max);
        }

        struct TriStim {
            float r;
            float g;
            float b;
        };

        inline float Luma(TriStim &stim) {
            return stim.r * 0.2627f + stim.g * 0.6780f + stim.b * 0.0593f;
        }

        inline float Luma(float r, float g, float b) {
            return r * 0.2627f + g * 0.6780f + b * 0.0593f;
        }

        inline static float ToLinearToneMap(float v) {
            v = std::max(0.0f, v);
            return std::min(2.3f * pow(v, 2.8f), v / 5.0f + 0.8f);
        }

        inline float ToLinearPQ(float v) {
            v = fmax(0.0f, v);
            float m1 = (2610.0f / 4096.0f) / 4.0f;
            float m2 = (2523.0f / 4096.0f) * 128.0f;
            float c1 = 3424.0f / 4096.0f;
            float c2 = (2413.0f / 4096.0f) * 32.0f;
            float c3 = (2392.0f / 4096.0f) * 32.0f;
            float p = pow(v, 1.0f / m2);
            v = powf(fmax(p - c1, 0.0f) / (c2 - c3 * p), 1.0f / m1);
            v *= 10000.0f / 80.0f;
            return v;
        }

        template<class D, typename T = Vec<D>>
        inline T dciP3PQGammaCorrection(const D d, T color) {
            return PowPQ<float32_t, 4>(color, Set(d, 1 / 2.6f));
        }

        inline float dciP3PQGammaCorrection(float linear) {
            return powf(linear, 1 / 2.6f);
        }

        inline void TransferROWU16HFloats(uint16_t *data, PQGammaCorrection gammaCorrection) {
            auto r = (float) LoadHalf(data[0]);
            auto g = (float) LoadHalf(data[1]);
            auto b = (float) LoadHalf(data[2]);
            float luma = Luma(ToLinearToneMap(r), ToLinearToneMap(g), ToLinearToneMap(b));
            TriStim smpte = {ToLinearPQ(r), ToLinearPQ(g), ToLinearPQ(b)};
            float pqLuma = Luma(smpte);
            float scale = luma / pqLuma;
            if (gammaCorrection == Rec2020) {
                data[0] = half((float) bt2020GammaCorrection(smpte.r * scale)).data_;
                data[1] = half((float) bt2020GammaCorrection(smpte.g * scale)).data_;
                data[2] = half((float) bt2020GammaCorrection(smpte.b * scale)).data_;
            } else if (gammaCorrection == DCIP3) {
                data[0] = half((float) dciP3PQGammaCorrection(smpte.r * scale)).data_;
                data[1] = half((float) dciP3PQGammaCorrection(smpte.g * scale)).data_;
                data[2] = half((float) dciP3PQGammaCorrection(smpte.b * scale)).data_;
            }
        }

        inline void
        TransferROWU8(uint8_t *data, float maxColors, PQGammaCorrection gammaCorrection) {
            auto r = (float) data[0] / (float) maxColors;
            auto g = (float) data[1] / (float) maxColors;
            auto b = (float) data[2] / (float) maxColors;
            TriStim smpte = {ToLinearPQ(r), ToLinearPQ(g), ToLinearPQ(b)};
            if (gammaCorrection == Rec2020) {
                data[0] = (uint8_t) clamp((float) bt2020GammaCorrection(smpte.r * maxColors), 0,
                                          maxColors);
                data[1] = (uint8_t) clamp((float) bt2020GammaCorrection(smpte.g * maxColors), 0,
                                          maxColors);
                data[2] = (uint8_t) clamp((float) bt2020GammaCorrection(smpte.b * maxColors), 0,
                                          maxColors);
            } else if (gammaCorrection == DCIP3) {
                data[0] = (uint8_t) clamp((float) dciP3PQGammaCorrection(smpte.r * maxColors), 0,
                                          maxColors);
                data[1] = (uint8_t) clamp((float) dciP3PQGammaCorrection(smpte.g * maxColors), 0,
                                          maxColors);
                data[2] = (uint8_t) clamp((float) dciP3PQGammaCorrection(smpte.b * maxColors), 0,
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
                    Sub(Mul(alpha2020, PowPQ<float, 4>(color, Set(d, 0.45f))),
                        Sub(alpha2020, Set(d, 1.0f)));
            return IfThenElse(cmp, branch1, branch2);
        }

        template<class D, typename T = Vec<D>>
        inline T Luma(const D d, T r, T g, T b) {
            return Add(Add(Mul(r, Set(d, 0.2627f)), Mul(g, Set(d, 0.6780f))),
                       Mul(b, Set(d, 0.0593f)));
        }

        template<class D, typename T = Vec<D>>
        inline T ToLinearToneMap(const D d, T v) {
            v = Max(Zero(d), v);
            auto divSet = Set(d, 5.0f);
            return Min(Mul(PowPQ<float, 4>(v, Set(d, 2.8f)), Set(d, 2.3f)),
                       Add(Div(v, divSet), Set(d, 0.8f)));
        }

        template<class D, typename T = Vec<D>>
        inline T ToLinearPQ(const D d, T v) {
            v = Max(Zero(d), v);
            float m1 = (2610.0f / 4096.0f) / 4.0f;
            float m2 = (2523.0f / 4096.0f) * 128.0f;
            T c1 = Set(d, 3424.0f / 4096.0f);
            T c2 = Set(d, (2413.0f / 4096.0f) * 32.0f);
            T c3 = Set(d, (2392.0f / 4096.0f) * 32.0f);
            T p = PowPQ<float, 4>(v, Set(d, 1.0f / m2));
            v = PowPQ<float, 4>(Div(Max(Sub(p, c1), Zero(d)), (Sub(c2, Mul(c3, p)))),
                                Set(d, 1.0f / m1));
            v = Mul(v, Set(d, 10000.0f / 80.0f));
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

                VF32 rLinearTone = ToLinearToneMap(df32, r32);
                VF32 gLinearTone = ToLinearToneMap(df32, g32);
                VF32 bLinearTone = ToLinearToneMap(df32, b32);

                VF32 linearLuma = Luma(df32, rLinearTone, gLinearTone, bLinearTone);

                VF32 pqR = ToLinearPQ(df32, r32);
                VF32 pqG = ToLinearPQ(df32, g32);
                VF32 pqB = ToLinearPQ(df32, b32);

                VF32 pqLuma = Luma(df32, pqR, pqG, pqB);

                VF32 scale = Div(linearLuma, pqLuma);

                if (gammaCorrection == Rec2020) {
                    pqR = bt2020GammaCorrection(df32, Mul(pqR, scale));
                    pqG = bt2020GammaCorrection(df32, Mul(pqG, scale));
                    pqB = bt2020GammaCorrection(df32, Mul(pqB, scale));
                } else if (gammaCorrection == DCIP3) {
                    pqR = dciP3PQGammaCorrection(df32, Mul(pqR, scale));
                    pqG = dciP3PQGammaCorrection(df32, Mul(pqG, scale));
                    pqB = dciP3PQGammaCorrection(df32, Mul(pqB, scale));
                }

                VU16 rNew = BitCast(du16, DemoteTo(rebind16, pqR));
                VU16 gNew = BitCast(du16, DemoteTo(rebind16, pqG));
                VU16 bNew = BitCast(du16, DemoteTo(rebind16, pqB));

                StoreInterleaved4(rNew, gNew, bNew, AURow, du16,
                                  reinterpret_cast<uint16_t *>(ptr16));
                ptr16 += 4 * 4;
            }

            for (; x < width; ++x) {
                TransferROWU16HFloats(reinterpret_cast<uint16_t *>(ptr16), gammaCorrection);
                ptr16 += 4;
            }
        }

        void ProcessUSRow(uint8_t *HWY_RESTRICT data, int width, float maxColors,
                          PQGammaCorrection gammaCorrection) {
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

            int x;
            for (x = 0; x + pixels < width; x += pixels) {
                VU16 RURow;
                VU16 GURow;
                VU16 BURow;
                VU16 AURow;
                LoadInterleaved4(d, reinterpret_cast<uint8_t *>(ptr16), RURow, GURow, BURow, AURow);
                VF32 r32 = Div(ConvertTo(rebind32, PromoteTo(signed32, RURow)), vColors);
                VF32 g32 = Div(ConvertTo(rebind32, PromoteTo(signed32, GURow)), vColors);
                VF32 b32 = Div(ConvertTo(rebind32, PromoteTo(signed32, BURow)), vColors);

                VF32 rLinearTone = ToLinearToneMap(df32, r32);
                VF32 gLinearTone = ToLinearToneMap(df32, g32);
                VF32 bLinearTone = ToLinearToneMap(df32, b32);

                VF32 linearLuma = Luma(df32, rLinearTone, gLinearTone, bLinearTone);

                VF32 pqR = ToLinearPQ(df32, r32);
                VF32 pqG = ToLinearPQ(df32, g32);
                VF32 pqB = ToLinearPQ(df32, b32);

                VF32 pqLuma = Luma(df32, pqR, pqG, pqB);

                VF32 scale = Div(linearLuma, pqLuma);

                if (gammaCorrection == Rec2020) {
                    pqR = bt2020GammaCorrection(df32, Mul(pqR, scale));
                    pqG = bt2020GammaCorrection(df32, Mul(pqG, scale));
                    pqB = bt2020GammaCorrection(df32, Mul(pqB, scale));
                } else if (gammaCorrection == DCIP3) {
                    pqR = dciP3PQGammaCorrection(df32, Mul(pqR, scale));
                    pqG = dciP3PQGammaCorrection(df32, Mul(pqG, scale));
                    pqB = dciP3PQGammaCorrection(df32, Mul(pqB, scale));
                }

                pqR = Max(Min(Mul(pqR, vColors), vColors),
                          Zero(df32));
                pqG = Max(Min(Mul(pqG, vColors), vColors),
                          Zero(df32));
                pqB = Max(Min(Mul(pqB, vColors), vColors),
                          Zero(df32));

                VU16 rNew = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, pqR));
                VU16 gNew = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, pqG));
                VU16 bNew = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, pqB));

                StoreInterleaved4(rNew, gNew, bNew, AURow, d,
                                  reinterpret_cast<uint8_t *>(ptr16));
                ptr16 += 4 * 4;
            }

            for (; x < width; ++x) {
                TransferROWU8(reinterpret_cast<uint8_t *>(ptr16), maxColors, gammaCorrection);
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
    ThreadPool pool;
    std::vector<std::future<void>> results;

    for (int i = 0; i < this->height; ++i) {
        auto r = pool.enqueue(coder::ProcessCPURow, this->rgbaData, i, this->U16, this->halfFloats,
                              this->stride, this->width, maxColors, this->gammaCorrection);
        results.push_back(std::move(r));
    }

    for (auto &result: results) {
        result.wait();
    }
}

#endif
