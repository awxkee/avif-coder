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

#include "HDRTransferAdapter.h"
#include <vector>
#include <thread>
#include "imagebits/half.hpp"
#include "Eigen/Eigen"
#include "concurrency.hpp"

using namespace half_float;
using namespace std;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "HDRTransferAdapter.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "algo/math-inl.h"
#include "eotf-inl.h"
#include "imagebits/half.hpp"
#include "CmsApply-inl.h"

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

    using namespace hwy;
    using namespace hwy::HWY_NAMESPACE;
    using hwy::float16_t;

    static const float sdrReferencePoint = 203.0f;

    float half_to_float(const uint16_t x) {
        half f;
        f.data_ = x;
        return f;
    }

    HWY_FAST_MATH_INLINE void
    TransferROWU16HFloats(uint16_t *data,
                          const GamutTransferFunction function,
                          GammaCurve gammaCorrection,
                          ToneMapper<FixedTag<float, 4>> *toneMapper,
                          Eigen::Matrix3f *conversion,
                          const float gamma,
                          const bool useChromaticAdaptation) {
        auto r = (float) half_to_float(data[0]);
        auto g = (float) half_to_float(data[1]);
        auto b = (float) half_to_float(data[2]);

        const auto adopt = getBradfordAdaptation();

        switch (function) {
            case HLG:
                r = HLGEotf(r);
                g = HLGEotf(g);
                b = HLGEotf(b);
                break;
            case PQ:
                r = ToLinearPQ(r, sdrReferencePoint);
                g = ToLinearPQ(g, sdrReferencePoint);
                b = ToLinearPQ(b, sdrReferencePoint);
                break;
            case SMPTE428:
                r = SMPTE428Eotf(r);
                g = SMPTE428Eotf(g);
                b = SMPTE428Eotf(b);
                break;
            case EOTF_GAMMA: {
                float gammaValue = gamma;
                r = gammaOtf(r, gammaValue);
                g = gammaOtf(g, gammaValue);
                b = gammaOtf(b, gammaValue);
            }
                break;
            case EOTF_BT601: {
                r = Rec601Eotf(r);
                g = Rec601Eotf(g);
                b = Rec601Eotf(b);
            }
                break;
            case EOTF_BT709: {
                r = Rec709Eotf(r);
                g = Rec709Eotf(g);
                b = Rec709Eotf(b);
            }
                break;
            case EOTF_SRGB: {
                r = SRGBToLinear(r);
                g = SRGBToLinear(g);
                b = SRGBToLinear(b);
            }
                break;
            case EOTF_SMPTE240: {
                r = SMPTE428Eotf(r);
                g = SMPTE428Eotf(g);
                b = SMPTE428Eotf(b);
            }
                break;
            case EOTF_LOG100: {
                r = Log100Eotf(r);
                g = Log100Eotf(g);
                b = Log100Eotf(b);
            }
                break;
            case EOTF_LOG100SRT10: {
                r = Log100Sqrt10Eotf(r);
                g = Log100Sqrt10Eotf(g);
                b = Log100Sqrt10Eotf(b);
            }
                break;
            case EOTF_IEC_61966: {
                r = Iec61966Eotf(r);
                g = Iec61966Eotf(g);
                b = Iec61966Eotf(b);
            }
                break;
            case EOTF_BT1361: {
                r = Bt1361Eotf(r);
                g = Bt1361Eotf(g);
                b = Bt1361Eotf(b);
            }
                break;
            default:
                break;
        }

        if (toneMapper) {
            toneMapper->Execute(r, g, b);
        }

        if (conversion) {
            convertColorProfile(*conversion, r, g, b);
            if (useChromaticAdaptation) {
                Eigen::Vector3f color = {r, g, b};
                color = adopt * color;
                r = color.x();
                g = color.y();
                b = color.z();
            }
        }

        if (gammaCorrection == Rec2020) {
            data[0] = half((float) bt2020GammaCorrection(r)).data_;
            data[1] = half((float) bt2020GammaCorrection(g)).data_;
            data[2] = half((float) bt2020GammaCorrection(b)).data_;
        } else if (gammaCorrection == DCIP3) {
            data[0] = half((float) dciP3PQGammaCorrection(r)).data_;
            data[1] = half((float) dciP3PQGammaCorrection(g)).data_;
            data[2] = half((float) dciP3PQGammaCorrection(b)).data_;
        } else if (gammaCorrection == GAMMA) {
            const float gammaEval = 1.f / gamma;
            data[0] = half((float) gammaOtf(r, gammaEval)).data_;
            data[1] = half((float) gammaOtf(g, gammaEval)).data_;
            data[2] = half((float) gammaOtf(b, gammaEval)).data_;
        } else if (gammaCorrection == Rec709) {
            data[0] = half((float) LinearITUR709ToITUR709(r)).data_;
            data[1] = half((float) LinearITUR709ToITUR709(g)).data_;
            data[2] = half((float) LinearITUR709ToITUR709(b)).data_;
        } else if (gammaCorrection == sRGB) {
            data[0] = half((float) LinearSRGBTosRGB(r)).data_;
            data[1] = half((float) LinearSRGBTosRGB(g)).data_;
            data[2] = half((float) LinearSRGBTosRGB(b)).data_;
        } else {
            data[0] = half(static_cast<float>(r)).data_;
            data[1] = half(static_cast<float>(g)).data_;
            data[2] = half(static_cast<float>(b)).data_;
        }
    }

    HWY_FAST_MATH_INLINE void
    TransferROWU8(uint8_t *data, const float maxColors,
                  const GammaCurve gammaCorrection,
                  const GamutTransferFunction function,
                  ToneMapper<FixedTag<float, 4>> *toneMapper,
                  Eigen::Matrix3f *conversion,
                  const float gamma,
                  bool useChromaticAdaptation) {
        auto r = (float) data[0] / (float) maxColors;
        auto g = (float) data[1] / (float) maxColors;
        auto b = (float) data[2] / (float) maxColors;

        const auto adopt = getBradfordAdaptation();

        switch (function) {
            case HLG:
                r = HLGEotf(r);
                g = HLGEotf(g);
                b = HLGEotf(b);
                break;
            case PQ:
                r = ToLinearPQ(r, sdrReferencePoint);
                g = ToLinearPQ(g, sdrReferencePoint);
                b = ToLinearPQ(b, sdrReferencePoint);
                break;
            case SMPTE428:
                r = SMPTE428Eotf(r);
                g = SMPTE428Eotf(g);
                b = SMPTE428Eotf(b);
                break;
            case EOTF_GAMMA: {
                r = gammaOtf(r, gamma);
                g = gammaOtf(g, gamma);
                b = gammaOtf(b, gamma);
            }
                break;
            case EOTF_BT601: {
                r = Rec601Eotf(r);
                g = Rec601Eotf(g);
                b = Rec601Eotf(b);
            }
                break;
            case EOTF_BT709: {
                r = Rec601Eotf(r);
                g = Rec601Eotf(g);
                b = Rec601Eotf(b);
            }
                break;
            case EOTF_SRGB: {
                r = SRGBToLinear(r);
                g = SRGBToLinear(g);
                b = SRGBToLinear(b);
            }
                break;
            case EOTF_SMPTE240: {
                r = Smpte240Eotf(r);
                g = Smpte240Eotf(g);
                b = Smpte240Eotf(b);
            }
                break;
            case EOTF_LOG100: {
                r = Log100Eotf(r);
                g = Log100Eotf(g);
                b = Log100Eotf(b);
            }
                break;
            case EOTF_LOG100SRT10: {
                r = Log100Sqrt10Eotf(r);
                g = Log100Sqrt10Eotf(g);
                b = Log100Sqrt10Eotf(b);
            }
                break;
            case EOTF_IEC_61966: {
                r = Iec61966Eotf(r);
                g = Iec61966Eotf(g);
                b = Iec61966Eotf(b);
            }
                break;
            case EOTF_BT1361: {
                r = Bt1361Eotf(r);
                g = Bt1361Eotf(g);
                b = Bt1361Eotf(b);
            }
                break;
            default:
                break;
        }

        if (toneMapper) {
            toneMapper->Execute(r, g, b);
        }

        if (conversion) {
            convertColorProfile(*conversion, r, g, b);
            if (useChromaticAdaptation) {
                Eigen::Vector3f color = {r, g, b};
                color = adopt * color;
                r = color.x();
                g = color.y();
                b = color.z();
            }
        }

        if (gammaCorrection == Rec2020) {
            data[0] = (uint8_t) std::clamp((float) bt2020GammaCorrection(r) * maxColors, 0.0f,
                                           maxColors);
            data[1] = (uint8_t) std::clamp((float) bt2020GammaCorrection(g) * maxColors, 0.0f,
                                           maxColors);
            data[2] = (uint8_t) std::clamp((float) bt2020GammaCorrection(b) * maxColors, 0.0f,
                                           maxColors);
        } else if (gammaCorrection == DCIP3) {
            data[0] = (uint8_t) std::clamp((float) dciP3PQGammaCorrection(r) * maxColors, 0.0f,
                                           maxColors);
            data[1] = (uint8_t) std::clamp((float) dciP3PQGammaCorrection(g) * maxColors, 0.0f,
                                           maxColors);
            data[2] = (uint8_t) std::clamp((float) dciP3PQGammaCorrection(b) * maxColors, 0.0f,
                                           maxColors);
        } else if (gammaCorrection == GAMMA) {
            const float gammaEval = 1.f / gamma;
            data[0] = (uint8_t) std::clamp((float) gammaOtf(r, gammaEval) * maxColors, 0.0f,
                                           maxColors);
            data[1] = (uint8_t) std::clamp((float) gammaOtf(g, gammaEval) * maxColors, 0.0f,
                                           maxColors);
            data[2] = (uint8_t) std::clamp((float) gammaOtf(b, gammaEval) * maxColors, 0.0f,
                                           maxColors);
        } else if (gammaCorrection == Rec709) {
            data[0] = (uint8_t) std::clamp((float) LinearITUR709ToITUR709(r) * maxColors, 0.0f,
                                           maxColors);
            data[1] = (uint8_t) std::clamp((float) LinearITUR709ToITUR709(g) * maxColors, 0.0f,
                                           maxColors);
            data[2] = (uint8_t) std::clamp((float) LinearITUR709ToITUR709(b) * maxColors, 0.0f,
                                           maxColors);
        } else if (gammaCorrection == sRGB) {
            data[0] = (uint8_t) std::clamp((float) LinearSRGBTosRGB(r) * maxColors, 0.0f,
                                           maxColors);
            data[1] = (uint8_t) std::clamp((float) LinearSRGBTosRGB(g) * maxColors, 0.0f,
                                           maxColors);
            data[2] = (uint8_t) std::clamp((float) LinearSRGBTosRGB(b) * maxColors, 0.0f,
                                           maxColors);
        } else {
            data[0] = (uint8_t) std::clamp((float) r * maxColors, 0.0f, maxColors);
            data[1] = (uint8_t) std::clamp((float) g * maxColors, 0.0f, maxColors);
            data[2] = (uint8_t) std::clamp((float) b * maxColors, 0.0f, maxColors);
        }
    }

    void
    ProcessF16Row(uint16_t *HWY_RESTRICT data, const int width,
                  const GammaCurve gammaCorrection, const GamutTransferFunction function,
                  CurveToneMapper curveToneMapper,
                  Eigen::Matrix3f *conversion,
                  const float gamma,
                  const bool useChromaticAdaptation) {
        const FixedTag<hwy::float16_t, 4> df16x4;
        const FixedTag<hwy::float16_t, 8> df16x8;
        const FixedTag<hwy::float32_t, 4> df32;
        const FixedTag<uint16_t, 4> du16x4;
        const FixedTag<uint16_t, 8> du16x8;
        const Rebind<hwy::float32_t, decltype(df16x4)> rebind32;
        const Rebind<hwy::float16_t, decltype(df32)> rebind16;

        using VU16x4 = Vec<decltype(du16x4)>;
        using VU16x8 = Vec<decltype(du16x8)>;
        using VF16x8 = Vec<decltype(df16x8)>;
        using VF32 = Vec<decltype(df32)>;

        const float lumaPrimaries[3] = {0.2627f, 0.6780f, 0.0593f};
        unique_ptr<ToneMapper<FixedTag<float, 4>>> toneMapper;
        if (curveToneMapper == LOGARITHMIC) {
            toneMapper.reset(new LogarithmicToneMapper<FixedTag<float, 4>>(lumaPrimaries));
        } else if (curveToneMapper == REC2408) {
            toneMapper.reset(new Rec2408PQToneMapper<FixedTag<float, 4>>(1000,
                                                                         250.0f, sdrReferencePoint,
                                                                         lumaPrimaries));
        }

        auto ptr16 = reinterpret_cast<hwy::float16_t *>(data);

        int pixels = 8;

        int x = 0;
        for (x = 0; x + pixels < width; x += pixels) {
            VU16x8 RURow;
            VU16x8 GURow;
            VU16x8 BURow;
            VU16x8 AURow;
            LoadInterleaved4(du16x8, reinterpret_cast<uint16_t *>(ptr16), RURow, GURow, BURow,
                             AURow);
            VF16x8 rf16 = BitCast(df16x8, RURow);
            VF16x8 gf16 = BitCast(df16x8, GURow);
            VF16x8 bf16 = BitCast(df16x8, BURow);
            VF32 rLow32 = PromoteLowerTo(rebind32, rf16);
            VF32 gLow32 = PromoteLowerTo(rebind32, gf16);
            VF32 bLow32 = PromoteLowerTo(rebind32, bf16);
            VF32 rHigh32 = PromoteUpperTo(rebind32, rf16);
            VF32 gHigh32 = PromoteUpperTo(rebind32, gf16);
            VF32 bHigh32 = PromoteUpperTo(rebind32, bf16);

            VF32 pqLowR;
            VF32 pqLowG;
            VF32 pqLowB;
            VF32 pqHighR;
            VF32 pqHighG;
            VF32 pqHighB;

            switch (function) {
                case PQ:
                    pqLowR = ToLinearPQ(df32, rLow32, sdrReferencePoint);
                    pqLowG = ToLinearPQ(df32, gLow32, sdrReferencePoint);
                    pqLowB = ToLinearPQ(df32, bLow32, sdrReferencePoint);
                    pqHighR = ToLinearPQ(df32, rHigh32, sdrReferencePoint);
                    pqHighG = ToLinearPQ(df32, gHigh32, sdrReferencePoint);
                    pqHighB = ToLinearPQ(df32, bHigh32, sdrReferencePoint);
                    break;
                case HLG:
                    pqLowR = HLGEotf(df32, rLow32);
                    pqLowG = HLGEotf(df32, gLow32);
                    pqLowB = HLGEotf(df32, bLow32);
                    pqHighR = HLGEotf(df32, rHigh32);
                    pqHighG = HLGEotf(df32, gHigh32);
                    pqHighB = HLGEotf(df32, bHigh32);
                    break;
                case SMPTE428:
                    pqLowR = SMPTE428Eotf(df32, rLow32);
                    pqLowG = SMPTE428Eotf(df32, gLow32);
                    pqLowB = SMPTE428Eotf(df32, bLow32);
                    pqHighR = SMPTE428Eotf(df32, rHigh32);
                    pqHighG = SMPTE428Eotf(df32, gHigh32);
                    pqHighB = SMPTE428Eotf(df32, bHigh32);
                    break;
                case EOTF_GAMMA: {
                    const float gammaValue = gamma;
                    pqLowR = gammaOtf(df32, rLow32, gammaValue);
                    pqLowG = gammaOtf(df32, gLow32, gammaValue);
                    pqLowB = gammaOtf(df32, bLow32, gammaValue);
                    pqHighR = gammaOtf(df32, rHigh32, gammaValue);
                    pqHighG = gammaOtf(df32, gHigh32, gammaValue);
                    pqHighB = gammaOtf(df32, bHigh32, gammaValue);
                }
                    break;
                case EOTF_BT601: {
                    pqLowR = Rec601Eotf(df32, rLow32);
                    pqLowG = Rec601Eotf(df32, gLow32);
                    pqLowB = Rec601Eotf(df32, bLow32);
                    pqHighR = Rec601Eotf(df32, rHigh32);
                    pqHighG = Rec601Eotf(df32, gHigh32);
                    pqHighB = Rec601Eotf(df32, bHigh32);
                }
                    break;
                case EOTF_BT709: {
                    pqLowR = Rec709Eotf(df32, rLow32);
                    pqLowG = Rec709Eotf(df32, gLow32);
                    pqLowB = Rec709Eotf(df32, bLow32);
                    pqHighR = Rec709Eotf(df32, rHigh32);
                    pqHighG = Rec709Eotf(df32, gHigh32);
                    pqHighB = Rec709Eotf(df32, bHigh32);
                }
                    break;
                case EOTF_SRGB: {
                    pqLowR = SRGBToLinear(df32, rLow32);
                    pqLowG = SRGBToLinear(df32, gLow32);
                    pqLowB = SRGBToLinear(df32, bLow32);
                    pqHighR = SRGBToLinear(df32, rHigh32);
                    pqHighG = SRGBToLinear(df32, gHigh32);
                    pqHighB = SRGBToLinear(df32, bHigh32);
                }
                    break;
                case EOTF_SMPTE240: {
                    pqLowR = Smpte240Eotf(df32, rLow32);
                    pqLowG = Smpte240Eotf(df32, gLow32);
                    pqLowB = Smpte240Eotf(df32, bLow32);
                    pqHighR = Smpte240Eotf(df32, rHigh32);
                    pqHighG = Smpte240Eotf(df32, gHigh32);
                    pqHighB = Smpte240Eotf(df32, bHigh32);
                }
                    break;
                case EOTF_LOG100: {
                    pqLowR = Log100Eotf(df32, rLow32);
                    pqLowG = Log100Eotf(df32, gLow32);
                    pqLowB = Log100Eotf(df32, bLow32);
                    pqHighR = Log100Eotf(df32, rHigh32);
                    pqHighG = Log100Eotf(df32, gHigh32);
                    pqHighB = Log100Eotf(df32, bHigh32);
                }
                    break;
                case EOTF_LOG100SRT10: {
                    pqLowR = Log100Sqrt10Eotf(df32, rLow32);
                    pqLowG = Log100Sqrt10Eotf(df32, gLow32);
                    pqLowB = Log100Sqrt10Eotf(df32, bLow32);
                    pqHighR = Log100Sqrt10Eotf(df32, rHigh32);
                    pqHighG = Log100Sqrt10Eotf(df32, gHigh32);
                    pqHighB = Log100Sqrt10Eotf(df32, bHigh32);
                }
                    break;
                case EOTF_IEC_61966: {
                    pqLowR = Iec61966Eotf(df32, rLow32);
                    pqLowG = Iec61966Eotf(df32, gLow32);
                    pqLowB = Iec61966Eotf(df32, bLow32);
                    pqHighR = Iec61966Eotf(df32, rHigh32);
                    pqHighG = Iec61966Eotf(df32, gHigh32);
                    pqHighB = Iec61966Eotf(df32, bHigh32);
                }
                    break;
                case EOTF_BT1361: {
                    pqLowR = Bt1361Eotf(df32, rLow32);
                    pqLowG = Bt1361Eotf(df32, gLow32);
                    pqLowB = Bt1361Eotf(df32, bLow32);
                    pqHighR = Bt1361Eotf(df32, rHigh32);
                    pqHighG = Bt1361Eotf(df32, gHigh32);
                    pqHighB = Bt1361Eotf(df32, bHigh32);
                }
                    break;
                default:
                    pqLowR = rLow32;
                    pqLowG = gLow32;
                    pqLowB = bLow32;
                    pqHighR = rHigh32;
                    pqHighG = gHigh32;
                    pqHighB = bHigh32;
            }

            if (toneMapper) {
                toneMapper->Execute(pqLowR, pqLowG, pqLowB);
                toneMapper->Execute(pqHighR, pqHighG, pqHighB);
            }

            const auto adopt = getBradfordAdaptation();

            if (conversion) {
                convertColorProfile(df32, *conversion, pqLowR, pqLowG, pqLowB);
                convertColorProfile(df32, *conversion, pqHighR, pqHighG, pqHighB);
                if (useChromaticAdaptation) {
                    convertColorProfile(df32, adopt, pqLowR, pqLowG, pqLowB);
                    convertColorProfile(df32, adopt, pqHighR, pqHighG, pqHighB);
                }
            }

            if (gammaCorrection == Rec2020) {
                pqLowR = bt2020GammaCorrection(df32, pqLowR);
                pqLowG = bt2020GammaCorrection(df32, pqLowG);
                pqLowB = bt2020GammaCorrection(df32, pqLowB);

                pqHighR = bt2020GammaCorrection(df32, pqHighR);
                pqHighG = bt2020GammaCorrection(df32, pqHighG);
                pqHighB = bt2020GammaCorrection(df32, pqHighB);
            } else if (gammaCorrection == DCIP3) {
                pqLowR = dciP3PQGammaCorrection(df32, pqLowR);
                pqLowG = dciP3PQGammaCorrection(df32, pqLowG);
                pqLowB = dciP3PQGammaCorrection(df32, pqLowB);

                pqHighR = dciP3PQGammaCorrection(df32, pqHighR);
                pqHighG = dciP3PQGammaCorrection(df32, pqHighG);
                pqHighB = dciP3PQGammaCorrection(df32, pqHighB);
            } else if (gammaCorrection == GAMMA) {
                const float gammaEval = 1.f / gamma;
                pqLowR = gammaOtf(df32, pqLowR, gammaEval);
                pqLowG = gammaOtf(df32, pqLowG, gammaEval);
                pqLowB = gammaOtf(df32, pqLowB, gammaEval);

                pqHighR = gammaOtf(df32, pqHighR, gammaEval);
                pqHighG = gammaOtf(df32, pqHighG, gammaEval);
                pqHighB = gammaOtf(df32, pqHighB, gammaEval);
            } else if (gammaCorrection == Rec709) {
                pqLowR = LinearITUR709ToITUR709(df32, pqLowR);
                pqLowG = LinearITUR709ToITUR709(df32, pqLowG);
                pqLowB = LinearITUR709ToITUR709(df32, pqLowB);

                pqHighR = LinearITUR709ToITUR709(df32, pqHighR);
                pqHighG = LinearITUR709ToITUR709(df32, pqHighG);
                pqHighB = LinearITUR709ToITUR709(df32, pqHighB);
            } else if (gammaCorrection == sRGB) {
                pqLowR = LinearSRGBTosRGB(df32, pqLowR);
                pqLowG = LinearSRGBTosRGB(df32, pqLowG);
                pqLowB = LinearSRGBTosRGB(df32, pqLowB);

                pqHighR = LinearSRGBTosRGB(df32, pqHighR);
                pqHighG = LinearSRGBTosRGB(df32, pqHighG);
                pqHighB = LinearSRGBTosRGB(df32, pqHighB);
            }

            VU16x4 rLowNew = BitCast(du16x4, DemoteTo(rebind16, pqLowR));
            VU16x4 gLowNew = BitCast(du16x4, DemoteTo(rebind16, pqLowG));
            VU16x4 bLowNew = BitCast(du16x4, DemoteTo(rebind16, pqLowB));

            VU16x4 rHighNew = BitCast(du16x4, DemoteTo(rebind16, pqHighR));
            VU16x4 gHighNew = BitCast(du16x4, DemoteTo(rebind16, pqHighG));
            VU16x4 bHighNew = BitCast(du16x4, DemoteTo(rebind16, pqHighB));

            StoreInterleaved4(Combine(du16x8, rHighNew, rLowNew),
                              Combine(du16x8, gHighNew, gLowNew),
                              Combine(du16x8, bHighNew, bLowNew),
                              AURow, du16x8,
                              reinterpret_cast<uint16_t *>(ptr16));
            ptr16 += 4 * pixels;
        }

        for (; x < width; ++x) {
            TransferROWU16HFloats(reinterpret_cast<uint16_t *>(ptr16), function,
                                  gammaCorrection,
                                  toneMapper.get(), conversion, gamma, useChromaticAdaptation);
            ptr16 += 4;
        }
    }

    template<class D, typename T = Vec<D>>
    void TransferU8Row(D df32,
                       const GammaCurve &gammaCorrection,
                       const GamutTransferFunction function,
                       ToneMapper<decltype(df32)> *toneMapper,
                       T &R,
                       T &G,
                       T &B,
                       const T vColors,
                       const T zeros,
                       Eigen::Matrix3f *conversion,
                       const float gamma,
                       const bool useChromaticAdaptation) {
        T pqR;
        T pqG;
        T pqB;

        switch (function) {
            case PQ:
                pqR = ToLinearPQ(df32, R, sdrReferencePoint);
                pqG = ToLinearPQ(df32, G, sdrReferencePoint);
                pqB = ToLinearPQ(df32, B, sdrReferencePoint);
                break;
            case HLG:
                pqR = HLGEotf(df32, R);
                pqG = HLGEotf(df32, G);
                pqB = HLGEotf(df32, B);
                break;
            case SMPTE428:
                pqR = SMPTE428Eotf(df32, R);
                pqG = SMPTE428Eotf(df32, G);
                pqB = SMPTE428Eotf(df32, B);
                break;
            case EOTF_GAMMA: {
                const float gammaValue = gamma;
                pqR = gammaOtf(df32, R, gammaValue);
                pqG = gammaOtf(df32, G, gammaValue);
                pqB = gammaOtf(df32, B, gammaValue);
            }
                break;
            case EOTF_BT601: {
                pqR = Rec601Eotf(df32, R);
                pqG = Rec601Eotf(df32, G);
                pqB = Rec601Eotf(df32, B);
            }
                break;
            case EOTF_SRGB: {
                pqR = SRGBToLinear(df32, R);
                pqG = SRGBToLinear(df32, G);
                pqB = SRGBToLinear(df32, B);
            }
                break;
            case EOTF_BT709: {
                pqR = Rec709Eotf(df32, R);
                pqG = Rec709Eotf(df32, G);
                pqB = Rec709Eotf(df32, B);
            }
                break;
            case EOTF_SMPTE240: {
                pqR = Smpte240Eotf(df32, R);
                pqG = Smpte240Eotf(df32, G);
                pqB = Smpte240Eotf(df32, B);
            }
                break;
            case EOTF_LOG100: {
                pqR = Log100Eotf(df32, R);
                pqG = Log100Eotf(df32, G);
                pqB = Log100Eotf(df32, B);
            }
                break;
            case EOTF_LOG100SRT10: {
                pqR = Log100Sqrt10Eotf(df32, R);
                pqG = Log100Sqrt10Eotf(df32, G);
                pqB = Log100Sqrt10Eotf(df32, B);
            }
                break;
            case EOTF_IEC_61966: {
                pqR = Iec61966Eotf(df32, R);
                pqG = Iec61966Eotf(df32, G);
                pqB = Iec61966Eotf(df32, B);
            }
                break;
            case EOTF_BT1361: {
                pqR = Bt1361Eotf(df32, R);
                pqG = Bt1361Eotf(df32, G);
                pqB = Bt1361Eotf(df32, B);
            }
                break;
            default:
                pqR = R;
                pqG = G;
                pqB = B;
        }

        if (toneMapper) {
            toneMapper->Execute(pqR, pqG, pqB);
        }

        const auto adopt = getBradfordAdaptation();

        if (conversion) {
            convertColorProfile(df32, *conversion, pqR, pqG, pqB);
            if (useChromaticAdaptation) {
                convertColorProfile(df32, adopt, pqR, pqG, pqB);
            }
        }

        if (gammaCorrection == Rec2020) {
            pqR = bt2020GammaCorrection(df32, pqR);
            pqG = bt2020GammaCorrection(df32, pqG);
            pqB = bt2020GammaCorrection(df32, pqB);
        } else if (gammaCorrection == DCIP3) {
            pqR = dciP3PQGammaCorrection(df32, pqR);
            pqG = dciP3PQGammaCorrection(df32, pqG);
            pqB = dciP3PQGammaCorrection(df32, pqB);
        } else if (gammaCorrection == GAMMA) {
            const float gammaEval = 1.0f / gamma;
            pqR = gammaOtf(df32, pqR, gammaEval);
            pqG = gammaOtf(df32, pqG, gammaEval);
            pqB = gammaOtf(df32, pqB, gammaEval);
        } else if (gammaCorrection == Rec709) {
            pqR = LinearITUR709ToITUR709(df32, pqR);
            pqG = LinearITUR709ToITUR709(df32, pqG);
            pqB = LinearITUR709ToITUR709(df32, pqB);
        } else if (gammaCorrection == sRGB) {
            pqR = LinearSRGBTosRGB(df32, pqR);
            pqG = LinearSRGBTosRGB(df32, pqG);
            pqB = LinearSRGBTosRGB(df32, pqB);
        }

        pqR = Max(Min(Round(Mul(pqR, vColors)), vColors), zeros);
        pqG = Max(Min(Round(Mul(pqG, vColors)), vColors), zeros);
        pqB = Max(Min(Round(Mul(pqB, vColors)), vColors), zeros);

        R = pqR;
        G = pqG;
        B = pqB;
    }

    void ProcessUSRow(uint8_t *HWY_RESTRICT data, const int width, const float maxColors,
                      const GammaCurve gammaCorrection,
                      const GamutTransferFunction function,
                      CurveToneMapper curveToneMapper,
                      Eigen::Matrix3f *conversion,
                      const float gamma,
                      const bool useChromaticAdaptation) {
        const FixedTag<float32_t, 4> df32;
        FixedTag<uint8_t, 16> d;
        FixedTag<uint16_t, 8> du16;
        FixedTag<uint8_t, 8> du8x8;
        FixedTag<uint32_t, 4> du32;

        const Rebind<int32_t, decltype(df32)> floatToSigned;
        const Rebind<uint16_t, FixedTag<float32_t, 4>> rebindOrigin;

        unique_ptr<ToneMapper<FixedTag<float, 4>>> toneMapper;
        const float lumaPrimaries[3] = {0.2627f, 0.6780f, 0.0593f};
        if (curveToneMapper == LOGARITHMIC) {
            toneMapper.reset(new LogarithmicToneMapper<FixedTag<float, 4>>(lumaPrimaries));
        } else if (curveToneMapper == REC2408) {
            toneMapper.reset(new Rec2408PQToneMapper<FixedTag<float, 4>>(1000,
                                                                         250.0f, 203.0f,
                                                                         lumaPrimaries));
        }

        const Rebind<float32_t, decltype(du32)> rebind32;

        using VU8 = Vec<decltype(d)>;
        using VF32 = Vec<decltype(df32)>;

        auto ptr16 = reinterpret_cast<uint8_t *>(data);

        VF32 vColors = Set(df32, (float) maxColors);
        VF32 recProcColors = ApproximateReciprocal(vColors);
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

            VF32 rLowerLow32 = Mul(ConvertTo(rebind32, PromoteLowerTo(du32, lowR16)),
                                   recProcColors);
            VF32 gLowerLow32 = Mul(ConvertTo(rebind32, PromoteLowerTo(du32, lowG16)),
                                   recProcColors);
            VF32 bLowerLow32 = Mul(ConvertTo(rebind32, PromoteLowerTo(du32, lowB16)),
                                   recProcColors);

            TransferU8Row(df32, gammaCorrection, function, toneMapper.get(), rLowerLow32,
                          gLowerLow32,
                          bLowerLow32,
                          vColors, vZeros, conversion, gamma, useChromaticAdaptation);

            VF32 rLowerHigh32 = Mul(ConvertTo(rebind32, PromoteUpperTo(du32, lowR16)),
                                    recProcColors);
            VF32 gLowerHigh32 = Mul(ConvertTo(rebind32, PromoteUpperTo(du32, lowG16)),
                                    recProcColors);
            VF32 bLowerHigh32 = Mul(ConvertTo(rebind32, PromoteUpperTo(du32, lowB16)),
                                    recProcColors);

            TransferU8Row(df32, gammaCorrection, function, toneMapper.get(), rLowerHigh32,
                          gLowerHigh32,
                          bLowerHigh32,
                          vColors, vZeros, conversion, gamma, useChromaticAdaptation);

            auto upperR16 = PromoteUpperTo(du16, RURow);
            auto upperG16 = PromoteUpperTo(du16, GURow);
            auto upperB16 = PromoteUpperTo(du16, BURow);

            VF32 rHigherLow32 = Mul(ConvertTo(rebind32, PromoteLowerTo(du32, upperR16)),
                                    recProcColors);
            VF32 gHigherLow32 = Mul(ConvertTo(rebind32, PromoteLowerTo(du32, upperG16)),
                                    recProcColors);
            VF32 bHigherLow32 = Mul(ConvertTo(rebind32, PromoteLowerTo(du32, upperB16)),
                                    recProcColors);

            TransferU8Row(df32, gammaCorrection, function, toneMapper.get(), rHigherLow32,
                          gHigherLow32,
                          bHigherLow32,
                          vColors, vZeros, conversion, gamma, useChromaticAdaptation);

            VF32 rHigherHigh32 = Mul(ConvertTo(rebind32, PromoteUpperTo(du32, upperR16)),
                                     recProcColors);
            VF32 gHigherHigh32 = Mul(ConvertTo(rebind32, PromoteUpperTo(du32, upperG16)),
                                     recProcColors);
            VF32 bHigherHigh32 = Mul(ConvertTo(rebind32, PromoteUpperTo(du32, upperB16)),
                                     recProcColors);

            TransferU8Row(df32, gammaCorrection, function, toneMapper.get(), rHigherHigh32,
                          gHigherHigh32,
                          bHigherHigh32, vColors, vZeros, conversion, gamma,
                          useChromaticAdaptation);

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
                          function,
                          toneMapper.get(), conversion, gamma, useChromaticAdaptation);
            ptr16 += 4;
        }
    }

    void ProcessCPURowHWY(uint8_t *data, int y, bool halfFloats,
                          int stride, const int width,
                          float maxColors,
                          const GammaCurve gammaCorrection,
                          const GamutTransferFunction function,
                          CurveToneMapper curveToneMapper,
                          Eigen::Matrix3f *conversion,
                          const float gamma,
                          const bool useChromaticAdaptation) {

        if (halfFloats) {
            auto ptr16 = reinterpret_cast<uint16_t *>(data + y * stride);
            ProcessF16Row(reinterpret_cast<uint16_t *>(ptr16), width,
                          gammaCorrection, function, curveToneMapper,
                          conversion, gamma, useChromaticAdaptation);
        } else {
            auto ptr16 = reinterpret_cast<uint8_t *>(data + y * stride);
            ProcessUSRow(reinterpret_cast<uint8_t *>(ptr16),
                         width,
                         (float) maxColors, gammaCorrection, function,
                         curveToneMapper, conversion, gamma,
                         useChromaticAdaptation);
        }
    }

    void ProcessGamutHighway(uint8_t *data, const int width, const int height, bool halfFloats,
                             int stride,
                             float maxColors,
                             const GammaCurve gammaCorrection,
                             const GamutTransferFunction function,
                             CurveToneMapper curveToneMapper,
                             Eigen::Matrix3f *conversion,
                             const float gamma,
                             const bool useChromaticAdaptation) {
        const int threadCount = std::clamp(
                std::min(static_cast<int>(std::thread::hardware_concurrency()),
                         height * width / (256 * 256)), 1, 12);
        concurrency::parallel_for(threadCount, height, [&](int y) {
            ProcessCPURowHWY(data, y, halfFloats,
                             stride, width, maxColors, gammaCorrection,
                             function, curveToneMapper, conversion, gamma,
                             useChromaticAdaptation);
        });
    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(ProcessGamutHighway);
    HWY_DLLEXPORT void
    ProcessCPUDispatcher(uint8_t *data, const int width, const int height, bool halfFloats,
                         int stride,
                         float maxColors,
                         const GammaCurve gammaCorrection,
                         const GamutTransferFunction function,
                         CurveToneMapper curveToneMapper,
                         Eigen::Matrix3f *conversion,
                         const float gamma,
                         const bool useChromaticAdaptation) {
        HWY_DYNAMIC_DISPATCH(ProcessGamutHighway)(data, width, height, halfFloats, stride,
                                                  maxColors,
                                                  gammaCorrection,
                                                  function, curveToneMapper, conversion, gamma,
                                                  useChromaticAdaptation);
    }
}

void HDRTransferAdapter::transfer() {
    auto maxColors = std::pow(2, (float) this->bitDepth) - 1;
    coder::ProcessCPUDispatcher(this->rgbaData, this->width, this->height, this->halfFloats,
                                this->stride, maxColors, this->gammaCorrection,
                                this->function, this->toneMapper,
                                this->mColorProfileConversion,
                                this->gamma, this->useChromaticAdaptation);
}

#endif
