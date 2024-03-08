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

#include "GamutAdapter.h"
#include <vector>
#include <thread>
#include "imagebits/half.hpp"
#include "Eigen/Eigen"
#include "concurrency.hpp"

using namespace half_float;
using namespace std;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "colorspace/GamutAdapter.cpp"

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

template<class D>
struct ChromaAdaptation {
  ChromaAdaptation(D d,
                   Eigen::Matrix3f *conversion,
                   CurveToneMapper curveToneMapper,
                   const float gamma,
                   const bool useChromaticAdaptation,
                   const float maxColors) :
      d(d),
      conversion(conversion),
      gamma(gamma),
      useChromaticAdaptation(useChromaticAdaptation),
      maxColors(maxColors),
      scaleColors(1.f / maxColors) {
    const float lumaPrimaries[3] = {0.2627f, 0.6780f, 0.0593f};
    if (curveToneMapper == LOGARITHMIC) {
      mapper.reset(new LogarithmicToneMapper<Rebind<hwy::float32_t, decltype(d)>>(
          lumaPrimaries));
    } else if (curveToneMapper == REC2408) {
      mapper.reset(new Rec2408PQToneMapper<Rebind<hwy::float32_t, decltype(d)>>(1000,
                                                                                250.0f,
                                                                                203.0f,
                                                                                lumaPrimaries));
    } else {
      mapper.reset(new BlackholeToneMapper<Rebind<hwy::float32_t, decltype(d)>>());
    }
  }

  template<typename T = Vec<Rebind<hwy::float32_t, D>>>
  void TransferRow(const GammaCurve &gammaCorrection,
                   const GamutTransferFunction function,
                   T &R,
                   T &G,
                   T &B) {

    T pqR;
    T pqG;
    T pqB;

    const Rebind<hwy::float32_t, D> df32;

    const auto zeros = Zero(df32);

    if (std::is_same<TFromD<D>, uint8_t>::value ||
        std::is_same<TFromD<D>, uint16_t>::value) {
      const auto vScaleColors = Set(df32, scaleColors);
      R = Mul(R, vScaleColors);
      G = Mul(G, vScaleColors);
      B = Mul(B, vScaleColors);
    }

    switch (function) {
      case PQ: {
        pqR = ToLinearPQ(df32, R, sdrReferencePoint);
        pqG = ToLinearPQ(df32, G, sdrReferencePoint);
        pqB = ToLinearPQ(df32, B, sdrReferencePoint);
      }
        break;
      case HLG: {
        pqR = HLGEotf(df32, R);
        pqG = HLGEotf(df32, G);
        pqB = HLGEotf(df32, B);
      }
        break;
      case SMPTE428: {
        pqR = SMPTE428Eotf(df32, R);
        pqG = SMPTE428Eotf(df32, G);
        pqB = SMPTE428Eotf(df32, B);
      }
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
        pqR = SRGBEotf(df32, R);
        pqG = SRGBEotf(df32, G);
        pqB = SRGBEotf(df32, B);
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
      default: {
        pqR = R;
        pqG = G;
        pqB = B;
      }
        break;
    }

    mapper->Execute(pqR, pqG, pqB);

    if (conversion) {
      convertColorProfile(df32, *conversion, pqR, pqG, pqB);
      if (useChromaticAdaptation) {
        const auto adopt = getBradfordAdaptation();
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
      pqR = SRGBOetf(df32, pqR);
      pqG = SRGBOetf(df32, pqG);
      pqB = SRGBOetf(df32, pqB);
    }

    if (std::is_same<TFromD<D>, uint8_t>::value ||
        std::is_same<TFromD<D>, uint16_t>::value) {
      const auto vColors = Set(df32, maxColors);
      pqR = Clamp(Round(Mul(pqR, vColors)), zeros, vColors);
      pqG = Clamp(Round(Mul(pqG, vColors)), zeros, vColors);
      pqB = Clamp(Round(Mul(pqB, vColors)), zeros, vColors);
    }

    R = pqR;
    G = pqG;
    B = pqB;
  }

 private:
  const D d;
  const Eigen::Matrix3f *conversion;
  const float gamma;
  const bool useChromaticAdaptation;
  const float maxColors;
  const float scaleColors;

  unique_ptr<ToneMapper<Rebind<hwy::float32_t, decltype(d)>>> mapper;
};

template<class D, HWY_IF_U16_D(D)>
void
ProcessDoubleRow(D d, TFromD<D> *HWY_RESTRICT data, const int width,
                 const GammaCurve gammaCorrection, const GamutTransferFunction function,
                 CurveToneMapper curveToneMapper,
                 Eigen::Matrix3f *conversion,
                 const float gamma,
                 const bool useChromaticAdaptation,
                 const float maxColors) {
  const Rebind<TFromD<D>, Half<decltype(d)>> dHalf;
  const FixedTag<hwy::float32_t, 4> df32;
  const Rebind<hwy::float32_t, decltype(dHalf)> rebind32;

  using VHalf = Vec<decltype(dHalf)>;
  using VFull = Vec<decltype(d)>;
  using VF32 = Vec<decltype(df32)>;

  ChromaAdaptation<decltype(dHalf)> chromaAdaptation(dHalf, conversion, curveToneMapper,
                                                     gamma,
                                                     useChromaticAdaptation, maxColors);

  auto ptr16 = reinterpret_cast<TFromD<D> *>(data);

  const int pixels = 8;

  int x = 0;
  for (; x + pixels < width; x += pixels) {
    VFull RURow;
    VFull GURow;
    VFull BURow;
    VFull AURow;
    LoadInterleaved4(d, reinterpret_cast<TFromD<D> *>(ptr16), RURow, GURow, BURow, AURow);
    VF32 rLow32 = PromoteLowerTo(rebind32, RURow);
    VF32 gLow32 = PromoteLowerTo(rebind32, GURow);
    VF32 bLow32 = PromoteLowerTo(rebind32, BURow);
    VF32 rHigh32 = PromoteUpperTo(rebind32, RURow);
    VF32 gHigh32 = PromoteUpperTo(rebind32, GURow);
    VF32 bHigh32 = PromoteUpperTo(rebind32, BURow);

    chromaAdaptation.TransferRow(gammaCorrection, function, rLow32, gLow32, bLow32);
    chromaAdaptation.TransferRow(gammaCorrection, function, rHigh32, gHigh32, bHigh32);

    VHalf rLowNew = DemoteTo(dHalf, rLow32);
    VHalf gLowNew = DemoteTo(dHalf, gLow32);
    VHalf bLowNew = DemoteTo(dHalf, bLow32);

    VHalf rHighNew = DemoteTo(dHalf, rHigh32);
    VHalf gHighNew = DemoteTo(dHalf, gHigh32);
    VHalf bHighNew = DemoteTo(dHalf, bHigh32);

    StoreInterleaved4(Combine(d, rHighNew, rLowNew),
                      Combine(d, gHighNew, gLowNew),
                      Combine(d, bHighNew, bLowNew),
                      AURow, d,
                      reinterpret_cast<TFromD<D> *>(ptr16));
    ptr16 += 4 * pixels;
  }

  const FixedTag<TFromD<D>, 1> dFixed1;
  using V1 = Vec<decltype(dFixed1)>;
  const Rebind<hwy::float32_t, decltype(dFixed1)> f1;
  using VF1 = Vec<decltype(f1)>;

  ChromaAdaptation<decltype(dFixed1)> chromaAdaptation1(dFixed1, conversion, curveToneMapper,
                                                        gamma,
                                                        useChromaticAdaptation, maxColors);

  for (; x < width; ++x) {
    V1 RURow;
    V1 GURow;
    V1 BURow;
    V1 AURow;
    LoadInterleaved4(dFixed1, reinterpret_cast<TFromD<D> *>(ptr16), RURow, GURow, BURow,
                     AURow);
    VF1 rLow32 = PromoteTo(f1, RURow);
    VF1 gLow32 = PromoteTo(f1, GURow);
    VF1 bLow32 = PromoteTo(f1, BURow);

    chromaAdaptation1.TransferRow(gammaCorrection, function, rLow32, gLow32, bLow32);

    V1 rLowNew = DemoteTo(dFixed1, rLow32);
    V1 gLowNew = DemoteTo(dFixed1, gLow32);
    V1 bLowNew = DemoteTo(dFixed1, bLow32);

    StoreInterleaved4(rLowNew, gLowNew, bLowNew, AURow, dFixed1,
                      reinterpret_cast<TFromD<D> *>(ptr16));
    ptr16 += 4;
  }
}

template<class D, HWY_IF_F16_D(D)>
void
ProcessDoubleRow(D d, TFromD<D> *HWY_RESTRICT data, const int width,
                 const GammaCurve gammaCorrection, const GamutTransferFunction function,
                 CurveToneMapper curveToneMapper,
                 Eigen::Matrix3f *conversion,
                 const float gamma,
                 const bool useChromaticAdaptation,
                 const float maxColors) {
  const Rebind<TFromD<D>, Half<decltype(d)>> dHalf;
  const FixedTag<hwy::float32_t, 4> df32;
  const Rebind<hwy::float32_t, decltype(dHalf)> rebind32;
  const Rebind<uint16_t, decltype(d)> dStore;

  using VHalf = Vec<decltype(dHalf)>;
  using VFull = Vec<decltype(d)>;
  using VStore = Vec<decltype(dStore)>;
  using VF32 = Vec<decltype(df32)>;

  ChromaAdaptation<decltype(dHalf)> chromaAdaptation(dHalf, conversion, curveToneMapper,
                                                     gamma,
                                                     useChromaticAdaptation, maxColors);

  auto ptr16 = reinterpret_cast<TFromD<D> *>(data);

  const int pixels = 8;

  int x = 0;
  for (; x + pixels < width; x += pixels) {
    VStore RURow;
    VStore GURow;
    VStore BURow;
    VStore AURow;
    LoadInterleaved4(dStore, reinterpret_cast<uint16_t *>(ptr16), RURow, GURow, BURow,
                     AURow);

    VFull rFull = BitCast(d, RURow);
    VFull gFull = BitCast(d, GURow);
    VFull bFull = BitCast(d, BURow);

    VF32 rLow32 = PromoteLowerTo(rebind32, rFull);
    VF32 gLow32 = PromoteLowerTo(rebind32, gFull);
    VF32 bLow32 = PromoteLowerTo(rebind32, bFull);
    VF32 rHigh32 = PromoteUpperTo(rebind32, rFull);
    VF32 gHigh32 = PromoteUpperTo(rebind32, gFull);
    VF32 bHigh32 = PromoteUpperTo(rebind32, bFull);

    chromaAdaptation.TransferRow(gammaCorrection, function, rLow32, gLow32, bLow32);
    chromaAdaptation.TransferRow(gammaCorrection, function, rHigh32, gHigh32, bHigh32);

    VHalf rLowNew = DemoteTo(dHalf, rLow32);
    VHalf gLowNew = DemoteTo(dHalf, gLow32);
    VHalf bLowNew = DemoteTo(dHalf, bLow32);

    VHalf rHighNew = DemoteTo(dHalf, rHigh32);
    VHalf gHighNew = DemoteTo(dHalf, gHigh32);
    VHalf bHighNew = DemoteTo(dHalf, bHigh32);

    StoreInterleaved4(BitCast(dStore, Combine(d, rHighNew, rLowNew)),
                      BitCast(dStore, Combine(d, gHighNew, gLowNew)),
                      BitCast(dStore, Combine(d, bHighNew, bLowNew)),
                      AURow, dStore,
                      reinterpret_cast<uint16_t *>(ptr16));
    ptr16 += 4 * pixels;
  }

  const FixedTag<TFromD<D>, 1> dFixed1;
  const Rebind<uint16_t, decltype(dFixed1)> dUFixed1;
  using V1 = Vec<decltype(dFixed1)>;
  using VU1 = Vec<decltype(dUFixed1)>;
  const Rebind<hwy::float32_t, decltype(dFixed1)> f1;
  using VF1 = Vec<decltype(f1)>;

  ChromaAdaptation<decltype(dFixed1)> chromaAdaptation1(dFixed1, conversion, curveToneMapper,
                                                        gamma,
                                                        useChromaticAdaptation, maxColors);

  for (; x < width; ++x) {
    VU1 RURow;
    VU1 GURow;
    VU1 BURow;
    VU1 AURow;
    LoadInterleaved4(dUFixed1, reinterpret_cast<uint16_t *>(ptr16), RURow, GURow, BURow,
                     AURow);
    VF1 rLow32 = PromoteTo(f1, BitCast(dFixed1, RURow));
    VF1 gLow32 = PromoteTo(f1, BitCast(dFixed1, GURow));
    VF1 bLow32 = PromoteTo(f1, BitCast(dFixed1, BURow));

    chromaAdaptation1.TransferRow(gammaCorrection, function, rLow32, gLow32, bLow32);

    V1 rLowNew = DemoteTo(dFixed1, rLow32);
    V1 gLowNew = DemoteTo(dFixed1, gLow32);
    V1 bLowNew = DemoteTo(dFixed1, bLow32);

    StoreInterleaved4(BitCast(dUFixed1, rLowNew),
                      BitCast(dUFixed1, gLowNew),
                      BitCast(dUFixed1, bLowNew),
                      AURow, dUFixed1,
                      reinterpret_cast<uint16_t *>(ptr16));
    ptr16 += 4;
  }
}

void ProcessUSRow(uint8_t *HWY_RESTRICT data, const int width, const float maxColors,
                  const GammaCurve gammaCorrection,
                  const GamutTransferFunction function,
                  CurveToneMapper curveToneMapper,
                  Eigen::Matrix3f *conversion,
                  const float gamma,
                  const bool useChromaticAdaptation) {
  const FixedTag<float32_t, 4> df32;
  const FixedTag<uint8_t, 16> d;
  const FixedTag<uint16_t, 8> du16;
  const FixedTag<uint8_t, 8> du8x8;
  const FixedTag<uint32_t, 4> du32;

  const FixedTag<uint8_t, 4> du8x4;

  const Rebind<int32_t, decltype(df32)> floatToSigned;
  const Rebind<uint16_t, FixedTag<float32_t, 4>> rebindOrigin;

  const FixedTag<uint8_t, 1> du8x1;
  using VU8x1 = Vec<decltype(du8x1)>;
  const Rebind<float32_t, decltype(du8x1)> df32x1;
  using VF32x1 = Vec<decltype(df32x1)>;

  const Rebind<float32_t, decltype(du32)> rebind32;

  using VU8 = Vec<decltype(d)>;
  using VF32 = Vec<decltype(df32)>;

  auto ptr16 = reinterpret_cast<uint8_t *>(data);

  const int pixels = 4 * 4;

  ChromaAdaptation<decltype(du8x4)> chromaAdaptation(du8x4, conversion, curveToneMapper,
                                                     gamma,
                                                     useChromaticAdaptation, maxColors);

  int x = 0;
  for (; x + pixels < width; x += pixels) {
    VU8 RURow;
    VU8 GURow;
    VU8 BURow;
    VU8 AURow;
    LoadInterleaved4(d, reinterpret_cast<uint8_t *>(ptr16), RURow, GURow, BURow, AURow);
    auto lowR16 = PromoteLowerTo(du16, RURow);
    auto lowG16 = PromoteLowerTo(du16, GURow);
    auto lowB16 = PromoteLowerTo(du16, BURow);

    VF32 rLowerLow32 = ConvertTo(rebind32, PromoteLowerTo(du32, lowR16));
    VF32 gLowerLow32 = ConvertTo(rebind32, PromoteLowerTo(du32, lowG16));
    VF32 bLowerLow32 = ConvertTo(rebind32, PromoteLowerTo(du32, lowB16));

    chromaAdaptation.TransferRow(gammaCorrection,
                                 function,
                                 rLowerLow32,
                                 gLowerLow32,
                                 bLowerLow32);

    VF32 rLowerHigh32 = ConvertTo(rebind32, PromoteUpperTo(du32, lowR16));
    VF32 gLowerHigh32 = ConvertTo(rebind32, PromoteUpperTo(du32, lowG16));
    VF32 bLowerHigh32 = ConvertTo(rebind32, PromoteUpperTo(du32, lowB16));

    chromaAdaptation.TransferRow(gammaCorrection,
                                 function,
                                 rLowerHigh32,
                                 gLowerHigh32,
                                 bLowerHigh32);

    auto upperR16 = PromoteUpperTo(du16, RURow);
    auto upperG16 = PromoteUpperTo(du16, GURow);
    auto upperB16 = PromoteUpperTo(du16, BURow);

    VF32 rHigherLow32 = ConvertTo(rebind32, PromoteLowerTo(du32, upperR16));
    VF32 gHigherLow32 = ConvertTo(rebind32, PromoteLowerTo(du32, upperG16));
    VF32 bHigherLow32 = ConvertTo(rebind32, PromoteLowerTo(du32, upperB16));

    chromaAdaptation.TransferRow(gammaCorrection,
                                 function,
                                 rHigherLow32,
                                 gHigherLow32,
                                 bHigherLow32);

    VF32 rHigherHigh32 = ConvertTo(rebind32, PromoteUpperTo(du32, upperR16));
    VF32 gHigherHigh32 = ConvertTo(rebind32, PromoteUpperTo(du32, upperG16));
    VF32 bHigherHigh32 = ConvertTo(rebind32, PromoteUpperTo(du32, upperB16));

    chromaAdaptation.TransferRow(gammaCorrection,
                                 function,
                                 rHigherHigh32,
                                 gHigherHigh32,
                                 bHigherHigh32);

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

  ChromaAdaptation<decltype(du8x1)> chromaAdaptation1Pixel(du8x1, conversion, curveToneMapper,
                                                           gamma,
                                                           useChromaticAdaptation, maxColors);

  const VF32x1 recProcColors1 = Set(df32x1, 1.f / static_cast<float>(maxColors));

  for (; x < width; ++x) {
    VU8x1 RURow;
    VU8x1 GURow;
    VU8x1 BURow;
    VU8x1 AURow;
    LoadInterleaved4(du8x1, reinterpret_cast<uint8_t *>(ptr16), RURow, GURow, BURow, AURow);

    VF32x1 r = Mul(PromoteTo(df32x1, RURow), recProcColors1);
    VF32x1 g = Mul(PromoteTo(df32x1, GURow), recProcColors1);
    VF32x1 b = Mul(PromoteTo(df32x1, BURow), recProcColors1);

    chromaAdaptation1Pixel.TransferRow(gammaCorrection,
                                       function,
                                       r,
                                       g,
                                       b);

    RURow = DemoteTo(du8x1, r);
    GURow = DemoteTo(du8x1, g);
    BURow = DemoteTo(du8x1, b);

    StoreInterleaved4(RURow, GURow, BURow, AURow, du8x1,
                      reinterpret_cast<uint8_t *>(ptr16));

    ptr16 += 4;
  }
}

void ProcessGamutHighwayU16(uint16_t *data, const int width, const int height,
                            const int stride, const float maxColors,
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
    auto ptr16 = reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t *>(data) +
        y * stride);
    const FixedTag<uint16_t, 8> df;
    ProcessDoubleRow(df, reinterpret_cast<uint16_t *>(ptr16), width,
                     gammaCorrection, function, curveToneMapper,
                     conversion, gamma, useChromaticAdaptation, maxColors);
  });
}

void
ProcessGamutHighwayF16(hwy::float16_t *data, const int width, const int height,
                       const int stride, const float maxColors,
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
    auto ptr16 = reinterpret_cast<hwy::float16_t *>(reinterpret_cast<uint8_t *>(data) +
        y * stride);
    const FixedTag<hwy::float16_t, 8> df;
    ProcessDoubleRow(df, reinterpret_cast<hwy::float16_t *>(ptr16), width,
                     gammaCorrection, function, curveToneMapper,
                     conversion, gamma, useChromaticAdaptation, maxColors);
  });
}

void ProcessGamutHighwayU8(uint8_t *data, const int width, const int height,
                           const int stride, const float maxColors,
                           const GammaCurve gammaCorrection,
                           const GamutTransferFunction function,
                           const CurveToneMapper curveToneMapper,
                           Eigen::Matrix3f *conversion,
                           const float gamma,
                           const bool useChromaticAdaptation) {
  const int threadCount = std::clamp(
      std::min(static_cast<int>(std::thread::hardware_concurrency()),
               height * width / (256 * 256)), 1, 12);
  concurrency::parallel_for(threadCount, height, [&](int y) {
    auto ptr16 = reinterpret_cast<uint8_t *>(reinterpret_cast<uint8_t *>(data) + y * stride);
    ProcessUSRow(reinterpret_cast<uint8_t *>(ptr16),
                 width,
                 (float) maxColors, gammaCorrection, function,
                 curveToneMapper, conversion, gamma,
                 useChromaticAdaptation);
  });
}
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
HWY_EXPORT(ProcessGamutHighwayU8);
HWY_EXPORT(ProcessGamutHighwayF16);
HWY_EXPORT(ProcessGamutHighwayU16);

template<class T>
HWY_DLLEXPORT void
ProcessCPUDispatcher(T *data, const int width, const int height,
                     const int stride,
                     const float maxColors,
                     const GammaCurve gammaCorrection,
                     const GamutTransferFunction function,
                     const CurveToneMapper curveToneMapper,
                     Eigen::Matrix3f *conversion,
                     const float gamma,
                     const bool useChromaticAdaptation) {
  if (std::is_same<T, uint8_t>::value) {
    HWY_DYNAMIC_DISPATCH(ProcessGamutHighwayU8)(reinterpret_cast<uint8_t *>(data),
                                                width, height, stride,
                                                maxColors, gammaCorrection,
                                                function, curveToneMapper, conversion, gamma,
                                                useChromaticAdaptation);
  } else if (std::is_same<T, uint16_t>::value) {
    HWY_DYNAMIC_DISPATCH(ProcessGamutHighwayU16)(reinterpret_cast<uint16_t *>(data),
                                                 width, height, stride,
                                                 maxColors, gammaCorrection,
                                                 function, curveToneMapper, conversion, gamma,
                                                 useChromaticAdaptation);
  } else if (std::is_same<T, hwy::float16_t>::value) {
    HWY_DYNAMIC_DISPATCH(ProcessGamutHighwayF16)(reinterpret_cast<hwy::float16_t *>(data),
                                                 width, height, stride,
                                                 maxColors, gammaCorrection,
                                                 function, curveToneMapper, conversion, gamma,
                                                 useChromaticAdaptation);
  }
}

template void
ProcessCPUDispatcher(uint8_t *data, const int width, const int height,
                     const int stride,
                     const float maxColors,
                     const GammaCurve gammaCorrection,
                     const GamutTransferFunction function,
                     const CurveToneMapper curveToneMapper,
                     Eigen::Matrix3f *conversion,
                     const float gamma,
                     const bool useChromaticAdaptation);

template void
ProcessCPUDispatcher(uint16_t *data, const int width, const int height,
                     const int stride,
                     const float maxColors,
                     const GammaCurve gammaCorrection,
                     const GamutTransferFunction function,
                     const CurveToneMapper curveToneMapper,
                     Eigen::Matrix3f *conversion,
                     const float gamma,
                     const bool useChromaticAdaptation);

template void
ProcessCPUDispatcher(hwy::float16_t *data, const int width, const int height,
                     const int stride,
                     const float maxColors,
                     const GammaCurve gammaCorrection,
                     const GamutTransferFunction function,
                     const CurveToneMapper curveToneMapper,
                     Eigen::Matrix3f *conversion,
                     const float gamma,
                     const bool useChromaticAdaptation);
}

#endif
