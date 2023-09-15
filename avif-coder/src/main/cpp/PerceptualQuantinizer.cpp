//
// Created by Radzivon Bartoshyk on 07/09/2023.
//

#include "PerceptualQuantinizer.h"
#include "HalfFloats.h"
#include <vector>
#include "ThreadPool.hpp"

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

        static const float betaRec2020 = 0.018053968510807f;
        static const float alphaRec2020 = 1.09929682680944f;

        float bt2020GammaCorrection(float linear) {
            if (0 <= betaRec2020 && linear < betaRec2020) {
                return 4.5f * linear;
            } else if (betaRec2020 <= linear && linear < 1) {
                return alphaRec2020 * powf(linear, 0.45f) - (alphaRec2020 - 1.0f);
            }
            return linear;
        }

        float clamp(float value, float min, float max) {
            return std::min(std::max(value, min), max);
        }

        struct TriStim {
            float r;
            float g;
            float b;
        };


        float Luma(TriStim &stim) {
            return stim.r * 0.2627f + stim.g * 0.6780f + stim.b * 0.0593f;
        }

        float Luma(float r, float g, float b) {
            return r * 0.2627f + g * 0.6780f + b * 0.0593f;
        }

        static float ToLinearToneMap(float v) {
            v = std::max(0.0f, v);
            return std::min(2.3f * pow(v, 2.8f), v / 5.0f + 0.8f);
        }


        float ToLinearPQ(float v) {
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

        void TransferROWU16HFloats(uint16_t *data) {
            auto r = (float) half_to_float(data[0]);
            auto g = (float) half_to_float(data[1]);
            auto b = (float) half_to_float(data[2]);
            float luma = Luma(ToLinearToneMap(r), ToLinearToneMap(g), ToLinearToneMap(b));
            TriStim smpte = {ToLinearPQ(r), ToLinearPQ(g), ToLinearPQ(b)};
            float pqLuma = Luma(smpte);
            float scale = luma / pqLuma;
            data[0] = float_to_half((float) bt2020GammaCorrection(smpte.r * scale));
            data[1] = float_to_half((float) bt2020GammaCorrection(smpte.g * scale));
            data[2] = float_to_half((float) bt2020GammaCorrection(smpte.b * scale));
        }


        void TransferROWU8(uint8_t *data, float maxColors) {
            auto r = (float) data[0] / (float) maxColors;
            auto g = (float) data[1] / (float) maxColors;
            auto b = (float) data[2] / (float) maxColors;
            TriStim smpte = {ToLinearPQ(r), ToLinearPQ(g), ToLinearPQ(b)};
            data[0] = (uint8_t) clamp((float) smpte.r * maxColors, 0, maxColors);
            data[1] = (uint8_t) clamp((float) smpte.g * maxColors, 0, maxColors);
            data[2] = (uint8_t) clamp((float) smpte.b * maxColors, 0, maxColors);
        }

        template<class D, typename T = Vec<D>>
        inline T Pow(const D d, T x, float value) {
            T result = Zero(d);
            int lanes = d.MaxLanes();
            for (int i = 0; i < lanes; i++) {
                float r = powf((float) ExtractLane(x, i), value);
                result = InsertLane(result, i, r);
            }
            return result;
        }

        template<class D, typename T = Vec<D>>
        T bt2020GammaCorrection(const D d, T color) {
            T bt2020 = Set(d, betaRec2020);
            T alpha2020 = Set(d, alphaRec2020);
            const auto cmp = color < bt2020;
            const auto branch1 = Mul(color, Set(d, 4.5f));
            const auto branch2 =
                    Sub(Mul(alpha2020, Pow(d, color, 0.45f)), Sub(alpha2020, Set(d, 1.0f)));
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
            return Min(Mul(Pow(d, v, 2.8f), Set(d, 2.3f)), Add(Div(v, divSet), Set(d, 0.8f)));
        }

        template<class D, typename T = Vec<D>>
        inline T ToLinearPQ(const D d, T v) {
            v = Max(Zero(d), v);
            float m1 = (2610.0f / 4096.0f) / 4.0f;
            float m2 = (2523.0f / 4096.0f) * 128.0f;
            T c1 = Set(d, 3424.0f / 4096.0f);
            T c2 = Set(d, (2413.0f / 4096.0f) * 32.0f);
            T c3 = Set(d, (2392.0f / 4096.0f) * 32.0f);
            T p = Pow(d, v, 1.0f / m2);
            v = Pow(d, Div(Max(Sub(p, c1), Zero(d)), (Sub(c2, Mul(c3, p)))), 1.0f / m1);
            v = Mul(v, Set(d, 10000.0f / 80.0f));
            return v;
        }

        void ProcessF16Row(uint16_t *HWY_RESTRICT data, int width) {
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
                pqR = bt2020GammaCorrection(df32, Mul(pqR, scale));
                pqG = bt2020GammaCorrection(df32, Mul(pqG, scale));
                pqB = bt2020GammaCorrection(df32, Mul(pqB, scale));

                VU16 rNew = BitCast(du16, DemoteTo(rebind16, pqR));
                VU16 gNew = BitCast(du16, DemoteTo(rebind16, pqG));
                VU16 bNew = BitCast(du16, DemoteTo(rebind16, pqB));

                StoreInterleaved4(rNew, gNew, bNew, AURow, du16,
                                  reinterpret_cast<uint16_t *>(ptr16));
                ptr16 += 4 * 4;
            }

            for (; x < width; ++x) {
                TransferROWU16HFloats(reinterpret_cast<uint16_t *>(ptr16));
                ptr16 += 4;
            }
        }

        void ProcessUSRow(uint8_t *HWY_RESTRICT data, int width, float maxColors) {
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
                pqR = Max(Min(Mul(bt2020GammaCorrection(df32, Mul(pqR, scale)), vColors), vColors),
                          Zero(df32));
                pqG = Max(Min(Mul(bt2020GammaCorrection(df32, Mul(pqG, scale)), vColors), vColors),
                          Zero(df32));
                pqB = Max(Min(Mul(bt2020GammaCorrection(df32, Mul(pqB, scale)), vColors), vColors),
                          Zero(df32));

                VU16 rNew = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, pqR));
                VU16 gNew = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, pqG));
                VU16 bNew = DemoteTo(rebindOrigin, ConvertTo(floatToSigned, pqB));

                StoreInterleaved4(rNew, gNew, bNew, AURow, d,
                                  reinterpret_cast<uint8_t *>(ptr16));
                ptr16 += 4 * 4;
            }

            for (; x < width; ++x) {
                TransferROWU8(reinterpret_cast<uint8_t *>(ptr16), maxColors);
                ptr16 += 4;
            }
        }
    }
}

HWY_AFTER_NAMESPACE();

#if HAVE_NEON

#include <arm_neon.h>

/* Logarithm polynomial coefficients */
static const float32x4_t log_tab[] =
        {
                vdupq_n_f32(-2.29561495781f),
                vdupq_n_f32(-2.47071170807f),
                vdupq_n_f32(-5.68692588806f),
                vdupq_n_f32(-0.165253549814f),
                vdupq_n_f32(5.17591238022f),
                vdupq_n_f32(0.844007015228f),
                vdupq_n_f32(4.58445882797f),
                vdupq_n_f32(0.0141278216615f),
        };

static const uint32_t exp_f32_coeff[] =
        {
                0x3f7ffff6, // x^1: 0x1.ffffecp-1f
                0x3efffedb, // x^2: 0x1.fffdb6p-2f
                0x3e2aaf33, // x^3: 0x1.555e66p-3f
                0x3d2b9f17, // x^4: 0x1.573e2ep-5f
                0x3c072010, // x^5: 0x1.0e4020p-7f
        };

inline float32x4_t vtaylor_polyq_f32(float32x4_t x, const float32x4_t *coeffs) {
    float32x4_t A = vmlaq_f32(coeffs[0], coeffs[4], x);
    float32x4_t B = vmlaq_f32(coeffs[2], coeffs[6], x);
    float32x4_t C = vmlaq_f32(coeffs[1], coeffs[5], x);
    float32x4_t D = vmlaq_f32(coeffs[3], coeffs[7], x);
    float32x4_t x2 = vmulq_f32(x, x);
    float32x4_t x4 = vmulq_f32(x2, x2);
    float32x4_t res = vmlaq_f32(vmlaq_f32(A, B, x2), vmlaq_f32(C, D, x2), x4);
    return res;
}

inline float32x4_t prefer_vfmaq_f32(float32x4_t a, float32x4_t b, float32x4_t c) {
#if __ARM_FEATURE_FMA
    return vfmaq_f32(a, b, c);
#else // __ARM_FEATURE_FMA
    return vmlaq_f32(a, b, c);
#endif // __ARM_FEATURE_FMA
}

inline float32x4_t vexpq_f32(float32x4_t x) {
    const auto c1 = vreinterpretq_f32_u32(vdupq_n_u32(exp_f32_coeff[0]));
    const auto c2 = vreinterpretq_f32_u32(vdupq_n_u32(exp_f32_coeff[1]));
    const auto c3 = vreinterpretq_f32_u32(vdupq_n_u32(exp_f32_coeff[2]));
    const auto c4 = vreinterpretq_f32_u32(vdupq_n_u32(exp_f32_coeff[3]));
    const auto c5 = vreinterpretq_f32_u32(vdupq_n_u32(exp_f32_coeff[4]));

    const auto shift = vreinterpretq_f32_u32(
            vdupq_n_u32(0x4b00007f)); // 2^23 + 127 = 0x1.0000fep23f
    const auto inv_ln2 = vreinterpretq_f32_u32(
            vdupq_n_u32(0x3fb8aa3b)); // 1 / ln(2) = 0x1.715476p+0f
    const auto neg_ln2_hi = vreinterpretq_f32_u32(
            vdupq_n_u32(0xbf317200)); // -ln(2) from bits  -1 to -19: -0x1.62e400p-1f
    const auto neg_ln2_lo = vreinterpretq_f32_u32(
            vdupq_n_u32(0xb5bfbe8e)); // -ln(2) from bits -20 to -42: -0x1.7f7d1cp-20f

    const auto inf = vdupq_n_f32(std::numeric_limits<float>::infinity());
    const auto max_input = vdupq_n_f32(88.37f); // Approximately ln(2^127.5)
    const auto zero = vdupq_n_f32(0.f);
    const auto min_input = vdupq_n_f32(-86.64f); // Approximately ln(2^-125)

    // Range reduction:
    //   e^x = 2^n * e^r
    // where:
    //   n = floor(x / ln(2))
    //   r = x - n * ln(2)
    //
    // By adding x / ln(2) with 2^23 + 127 (shift):
    //   * As FP32 fraction part only has 23-bits, the addition of 2^23 + 127 forces decimal part
    //     of x / ln(2) out of the result. The integer part of x / ln(2) (i.e. n) + 127 will occupy
    //     the whole fraction part of z in FP32 format.
    //     Subtracting 2^23 + 127 (shift) from z will result in the integer part of x / ln(2)
    //     (i.e. n) because the decimal part has been pushed out and lost.
    //   * The addition of 127 makes the FP32 fraction part of z ready to be used as the exponent
    //     in FP32 format. Left shifting z by 23 bits will result in 2^n.
    const auto z = prefer_vfmaq_f32(shift, x, inv_ln2);
    const auto n = z - shift;
    const auto scale = vreinterpretq_f32_u32(vreinterpretq_u32_f32(z) << 23); // 2^n

    // The calculation of n * ln(2) is done using 2 steps to achieve accuracy beyond FP32.
    // This outperforms longer Taylor series (3-4 tabs) both in term of accuracy and performance.
    const auto r_hi = prefer_vfmaq_f32(x, n, neg_ln2_hi);
    const auto r = prefer_vfmaq_f32(r_hi, n, neg_ln2_lo);

    // Compute the truncated Taylor series of e^r.
    //   poly = scale * (1 + c1 * r + c2 * r^2 + c3 * r^3 + c4 * r^4 + c5 * r^5)
    const auto r2 = r * r;

    const auto p1 = c1 * r;
    const auto p23 = prefer_vfmaq_f32(c2, c3, r);
    const auto p45 = prefer_vfmaq_f32(c4, c5, r);
    const auto p2345 = prefer_vfmaq_f32(p23, p45, r2);
    const auto p12345 = prefer_vfmaq_f32(p1, p2345, r2);

    auto poly = prefer_vfmaq_f32(scale, p12345, scale);

    // Handle underflow and overflow.
    poly = vbslq_f32(vcltq_f32(x, min_input), zero, poly);
    poly = vbslq_f32(vcgtq_f32(x, max_input), inf, poly);

    return poly;
}

inline float32x4_t vlogq_f32(float32x4_t x) {
    static const int32x4_t CONST_127 = vdupq_n_s32(127);           // 127
    static const float32x4_t CONST_LN2 = vdupq_n_f32(0.6931471805f); // ln(2)

    // Extract exponent
    int32x4_t m = vsubq_s32(vreinterpretq_s32_u32(vshrq_n_u32(vreinterpretq_u32_f32(x), 23)),
                            CONST_127);
    float32x4_t val = vreinterpretq_f32_s32(
            vsubq_s32(vreinterpretq_s32_f32(x), vshlq_n_s32(m, 23)));

    // Polynomial Approximation
    float32x4_t poly = vtaylor_polyq_f32(val, log_tab);

    // Reconstruct
    poly = vmlaq_f32(poly, vcvtq_f32_s32(m), CONST_LN2);

    return poly;
}

inline float32x4_t vpowq_f32(float32x4_t val, float32x4_t n) {
    return vexpq_f32(vmulq_f32(n, vlogq_f32(val)));
}

inline float32x4_t vpowq_f32(float32x4_t t, float power) {
    return vpowq_f32(t, vdupq_n_f32(power));
}

#endif

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(ProcessUSRow);
    HWY_EXPORT(ProcessF16Row);
    HWY_DLLEXPORT void
    ProcessCPURow(uint8_t *data, int y, bool U16, bool halfFloats, int stride, int width,
                  float maxColors) {
        if (U16) {
            auto ptr16 = reinterpret_cast<uint16_t *>(data + y * stride);
            if (halfFloats) {
                HWY_DYNAMIC_DISPATCH(ProcessF16Row)(reinterpret_cast<uint16_t *>(ptr16), width);
            }
        } else {
            auto ptr16 = reinterpret_cast<uint8_t *>(data + y * stride);
            HWY_DYNAMIC_DISPATCH(ProcessUSRow)(reinterpret_cast<uint8_t *>(ptr16),
                                               width,
                                               (float) maxColors);
        }
    }
}

void PerceptualQuantinizer::transfer() {
    auto maxColors = powf(2, (float) this->bitDepth) - 1;
    ThreadPool pool;
    std::vector<std::future<void>> results;

    for (int i = 0; i < this->height; ++i) {
        auto r = pool.enqueue(coder::ProcessCPURow, this->rgbaData, i, this->U16, this->halfFloats,
                              this->stride, this->width, maxColors);
        results.push_back(std::move(r));
    }

    for (auto &result: results) {
        result.wait();
    }
}

#endif
