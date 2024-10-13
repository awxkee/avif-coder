/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 13/10/2024
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

#include "YuvConversion.h"
#include <algorithm>
#include <string>
#if HAVE_NEON
#include <arm_neon.h>
#endif

using namespace std;

static void ComputeForwardTransform(const float kr,
                                    const float kb,
                                    const float,
                                    const float,
                                    const float rangeY,
                                    const float rangeUV,
                                    const float fullRangeRGB,
                                    float &YR, float &YG, float &YB,
                                    float &Cb,
                                    float &Cr) {
  const float kg = 1.0f - kr - kb;
  if (kg == 0.f) {
    throw std::runtime_error("1.0f - kr - kg must not be 0");
  }

  YR = kr * rangeY / fullRangeRGB;
  YG = kg * rangeY / fullRangeRGB;
  YB = kb * rangeY / fullRangeRGB;

  Cb = 0.5f / (1.f - kb) * rangeUV / fullRangeRGB;
  Cr = 0.5f / (1.f - kr) * rangeUV / fullRangeRGB;
}

void ComputeForwardIntegersTransform(const float kr,
                                     const float kb,
                                     const float biasY,
                                     const float biasUV,
                                     const float rangeY,
                                     const float rangeUV,
                                     const float fullRangeRGB,
                                     const uint16_t precision,
                                     const uint16_t uvPrecision,
                                     uint16_t &uYR, uint16_t &uYG, uint16_t &uYB,
                                     uint16_t &kuCb, uint16_t &kuCr) {
  float YR, YG, YB;
  float kCb, kCr;

  const auto scale = static_cast<float>( 1 << precision );
  const auto scaleUV = static_cast<float>( 1 << uvPrecision );

  ComputeForwardTransform(kr, kb, static_cast<float>(biasY), static_cast<float>(biasUV),
                          static_cast<float>(rangeY), static_cast<float>(rangeUV),
                          fullRangeRGB, YR, YG, YB, kCb, kCr);

  uYR = static_cast<uint16_t>(::roundf(scale * YR));
  uYG = static_cast<uint16_t>(::roundf(scale * YG));
  uYB = static_cast<uint16_t>(::roundf(scale * YB));

  kuCb = static_cast<uint16_t>(::roundf(scaleUV * kCb));
  kuCr = static_cast<uint16_t>(::roundf(scaleUV * kCr));
}

void ComputeTransform(const float kr,
                      const float kb,
                      const float biasY,
                      const float biasUV,
                      const float rangeY,
                      const float rangeUV,
                      const float fullRangeRGB,
                      float &YR, float &YG, float &YB,
                      float &CbR, float &CbG, float &CbB,
                      float &CrR, float &CrG, float &CrB) {
  const float kg = 1.0f - kr - kb;
  if (kg == 0.f) {
    throw std::runtime_error("1.0f - kr - kg must not be 0");
  }

  YR = kr * rangeY / fullRangeRGB;
  YG = kg * rangeY / fullRangeRGB;
  YB = kb * rangeY / fullRangeRGB;

  CbR = -0.5f * kr / (1.f - kb) * rangeUV / fullRangeRGB;
  CbG = -0.5f * kg / (1.f - kb) * rangeUV / fullRangeRGB;
  CbB = 0.5f * rangeUV / fullRangeRGB;

  CrR = 0.5f * rangeUV / fullRangeRGB;
  CrG = -0.5f * kg / (1.f - kr) * rangeUV / fullRangeRGB;
  CrB = -0.5f * kb / (1.f - kr) * rangeUV / fullRangeRGB;
}

void ComputeTransformIntegers(const float kr,
                              const float kb,
                              const float biasY,
                              const float biasUV,
                              const float rangeY,
                              const float rangeUV,
                              const float fullRangeRGB,
                              const uint16_t precision,
                              int16_t &uYR, int16_t &uYG, int16_t &uYB,
                              int16_t &uCbR, int16_t &uCbG, int16_t &uCbB,
                              int16_t &uCrR, int16_t &uCrG, int16_t &uCrB) {
  float YR, YG, YB;
  float CbR, CbG, CbB;
  float CrR, CrG, CrB;

  const auto scale = static_cast<float>( 1 << precision );

  ComputeTransform(kr, kb, static_cast<float>(biasY), static_cast<float>(biasUV),
                   static_cast<float>(rangeY), static_cast<float>(rangeUV),
                   fullRangeRGB, YR, YG, YB, CbR, CbG, CbB, CrR, CrG, CrB);

  uYR = static_cast<int16_t>(::roundf(scale * YR));
  uYG = static_cast<int16_t>(::roundf(scale * YG));
  uYB = static_cast<int16_t>(::roundf(scale * YB));

  uCbR = static_cast<int16_t>(::roundf(scale * CbR));
  uCbG = static_cast<int16_t>(::roundf(scale * CbG));
  uCbB = static_cast<int16_t>(::roundf(scale * CbB));

  uCrR = static_cast<int16_t>(::roundf(scale * CrR));
  uCrG = static_cast<int16_t>(::roundf(scale * CrG));
  uCrB = static_cast<int16_t>(::roundf(scale * CrB));
}

void GetYUVRange(const YuvRange yuvColorRange, const int bitDepth,
                 uint16_t &biasY, uint16_t &biasUV,
                 uint16_t &rangeY, uint16_t &rangeUV) {
  if (yuvColorRange == YuvRange::Tv) {
    biasY = static_cast<uint16_t>(16 << (static_cast<uint16_t>(bitDepth) - 8));
    biasUV = static_cast<uint16_t>(1 << (static_cast<uint16_t>(bitDepth) - 1));
    rangeY = static_cast<uint16_t>((219 << (static_cast<uint16_t>(bitDepth) - 8)));
    rangeUV = static_cast<uint16_t>((224 << (static_cast<uint16_t>(bitDepth) - 8)));
  } else if (yuvColorRange == YuvRange::Full) {
    biasY = 0;
    biasUV = static_cast<uint16_t>(1 << (static_cast<uint16_t>(bitDepth) - 1));
    const auto maxColors = static_cast<uint16_t>(::powf(2.f, static_cast<float>(bitDepth)) - 1.f);
    rangeY = maxColors;
    rangeUV = maxColors;
  } else {
    std::string errorMessage = "Yuv Color Range must be valid parameter";
    throw std::runtime_error(errorMessage);
  }
}

YuvMatrixWeights getYuvWeights(YuvMatrix matrix) {
  switch (matrix) {
    case YuvMatrix::Bt2020:
      return YuvMatrixWeights{
          .kr = 0.2627,
          .kb = 0.0593,
      };
    case YuvMatrix::Bt709:
      return YuvMatrixWeights{
          .kr = 0.2126,
          .kb = 0.0722,
      };
    default:
      return YuvMatrixWeights{
          .kr = 0.299,
          .kb = 0.114,
      };
  }
}

void RgbaToYuv420(const uint8_t *sourceRgba, uint32_t sourceStride,
                  uint8_t *yPlane, uint32_t yStride,
                  uint8_t *uPlane, uint32_t uStride,
                  uint8_t *vPlane, uint32_t vStride,
                  uint32_t width, uint32_t height, YuvRange range, YuvMatrix matrix) {
  uint16_t biasY;
  uint16_t biasUV;
  uint16_t rangeY;
  uint16_t rangeUV;
  GetYUVRange(range, 8, biasY, biasUV, rangeY, rangeUV);

  int16_t YR, YG, YB;
  int16_t CbR, CbG, CbB;
  int16_t CrR, CrG, CrB;

  const int precision = 8;

  auto weights = getYuvWeights(matrix);

  ComputeTransformIntegers(weights.kr,
                           weights.kb,
                           static_cast<float>(biasY),
                           static_cast<float>(biasUV),
                           static_cast<float>(rangeY),
                           static_cast<float>(rangeUV),
                           255.f,
                           precision,
                           YR,
                           YG,
                           YB,
                           CbR,
                           CbG,
                           CbB,
                           CrR,
                           CrG,
                           CrB);

  const auto scale = static_cast<float>( 1 << precision );
  const auto iBiasY = static_cast<uint16_t>((static_cast<float>(biasY) + 0.5f) * scale);
  const auto iBiasUV = static_cast<uint16_t>((static_cast<float>(biasUV) + 0.5f) * scale);

  auto yStore = reinterpret_cast<uint8_t *>(yPlane);
  auto uStore = reinterpret_cast<uint8_t *>(uPlane);
  auto vStore = reinterpret_cast<uint8_t *>(vPlane);

  auto mSource = reinterpret_cast<const uint8_t *>(sourceRgba);

  int maxChroma = biasY + rangeUV;
  int maxLuma = biasY + rangeY;

  for (uint32_t y = 0; y < height; ++y) {

    auto yDst = reinterpret_cast<uint8_t *>(yStore);
    auto uDst = reinterpret_cast<uint8_t *>(uStore);
    auto vDst = reinterpret_cast<uint8_t *>(vStore);

    auto mSrc = reinterpret_cast<const uint8_t *>(mSource);

    uint32_t x = 0;

#if HAVE_NEON
    int16x8_t i_bias_y = vdupq_n_s16(static_cast<int16_t>(biasY));
    uint16x8_t i_cap_y = vdupq_n_u16(static_cast<uint16_t>(rangeY + biasY));
    int16x8_t i_cap_uv = vdupq_n_u16(static_cast<int16_t>(biasY + rangeUV));

    int16x8_t y_bias = vdupq_n_s32(iBiasY);
    int16x8_t uv_bias = vdupq_n_s32(iBiasUV);
    int16x8_t vYr = vdupq_n_s16(static_cast<int16_t>(YR));
    int16x8_t vYg = vdupq_n_s16(static_cast<int16_t>(YG));
    int16x8_t vYb = vdupq_n_s16(static_cast<int16_t>(YB));
    int16x8_t vCbR = vdupq_n_s16(static_cast<int16_t>(CbR));
    int16x8_t vCbG = vdupq_n_s16(static_cast<int16_t>(CbG));
    int16x8_t vCbB = vdupq_n_s16(static_cast<int16_t>(CbB));
    int16x8_t vCrR = vdupq_n_s16(static_cast<int16_t>(CrR));
    int16x8_t vCrG = vdupq_n_s16(static_cast<int16_t>(CrG));
    int16x8_t vCrB = vdupq_n_s16(static_cast<int16_t>(CrB));
    int32x4_t v_zeros = vdupq_n_s32(0);

    for (; x + 16 < width; x += 16) {
      uint8x16x4_t pixel = vld4q_u8(mSrc);

      int16x8_t r_high = vreinterpretq_s16_u16(vmovl_high_u8(pixel.val[0]));
      int16x8_t g_high = vreinterpretq_s16_u16(vmovl_high_u8(pixel.val[1]));
      int16x8_t b_high = vreinterpretq_s16_u16(vmovl_high_u8(pixel.val[2]));

      int16x4_t r_h_low = vget_low_s16(r_high);
      int16x4_t g_h_low = vget_low_s16(g_high);
      int16x4_t b_h_low = vget_low_s16(b_high);

      int32x4_t y_h_high = vmlal_high_s16(y_bias, r_high, vYr);
      y_h_high = vmlal_high_s16(y_h_high, g_high, vYg);
      y_h_high = vmlal_high_s16(y_h_high, b_high, vYb);
      y_h_high = vmaxq_s32(y_h_high, v_zeros);

      int32x4_t y_h_low = vmlal_s16(y_bias, r_h_low, vget_low_s16(vYr));
      y_h_low = vmlal_s16(y_h_low, g_h_low, vget_low_s16(vYg));
      y_h_low = vmlal_s16(y_h_low, b_h_low, vget_low_s16(vYb));
      y_h_low = vmaxq_s32(y_h_low, v_zeros);

      uint16x8_t y_high = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(y_h_low, 8),
                  vshrn_n_s32(y_h_high, 8)
              ),
              i_bias_y
          )),
          i_cap_y
      );

      int16x8_t r_low = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pixel.val[0])));
      int16x8_t g_low = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pixel.val[1])));
      int16x8_t b_low = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pixel.val[2])));

      int16x4_t r_l_low = vget_low_s16(r_low);
      int16x4_t g_l_low = vget_low_s16(g_low);
      int16x4_t b_l_low = vget_low_s16(b_low);

      int32x4_t y_l_high = vmlal_high_s16(y_bias, r_low, vYr);
      y_l_high = vmlal_high_s16(y_l_high, g_low, vYg);
      y_l_high = vmlal_high_s16(y_l_high, b_low, vYb);
      y_l_high = vmaxq_s32(y_l_high, v_zeros);

      int32x4_t y_l_low = vmlal_s16(y_bias, r_l_low, vget_low_s16(vYr));
      y_l_low = vmlal_s16(y_l_low, g_l_low, vget_low_s16(vYg));
      y_l_low = vmlal_s16(y_l_low, b_l_low, vget_low_s16(vYb));
      y_l_low = vmaxq_s32(y_l_low, v_zeros);

      uint16x8_t y_low = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(y_l_low, 8),
                  vshrn_n_s32(y_l_high, 8)
              ),
              i_bias_y
          )),
          i_cap_y
      );

      uint8x16_t yValues = vcombine_u8(vqmovn_u16(y_low), vqmovn_u16(y_high));
      vst1q_u8(yDst, yValues);

      if ((y & 1) == 0) {
        int32x4_t cb_h_high = vmlal_high_s16(uv_bias, r_high, vCbR);
        cb_h_high = vmlal_high_s16(cb_h_high, g_high, vCbG);
        cb_h_high = vmlal_high_s16(cb_h_high, b_high, vCbB);

        int32x4_t cb_h_low = vmlal_s16(uv_bias, r_h_low, vget_low_s16(vCbR));
        cb_h_low = vmlal_s16(cb_h_low, g_h_low, vget_low_s16(vCbG));
        cb_h_low = vmlal_s16(cb_h_low, b_h_low, vget_low_s16(vCbB));

        uint16x8_t cb_high = vminq_u16(
            vreinterpretq_u16_s16(vmaxq_s16(
                vcombine_s16(
                    vshrn_n_s32(cb_h_low, 8),
                    vshrn_n_s32(cb_h_high, 8)
                ),
                i_bias_y
            )),
            i_cap_uv
        );

        int32x4_t cr_h_high = vmlal_high_s16(uv_bias, r_high, vCrR);
        cr_h_high = vmlal_high_s16(cr_h_high, g_high, vCrG);
        cr_h_high = vmlal_high_s16(cr_h_high, b_high, vCrB);

        int32x4_t cr_h_low = vmlal_s16(uv_bias, r_h_low, vget_low_s16(vCrR));
        cr_h_low = vmlal_s16(cr_h_low, g_h_low, vget_low_s16(vCrG));
        cr_h_low = vmlal_s16(cr_h_low, b_h_low, vget_low_s16(vCrB));

        uint16x8_t cr_high = vminq_u16(
            vreinterpretq_u16_s16(vmaxq_s16(
                vcombine_s16(
                    vshrn_n_s32(cr_h_low, 8),
                    vshrn_n_s32(cr_h_high, 8)
                ),
                i_bias_y
            )),
            i_cap_uv
        );

        uint32x4_t cb_l_high = vmlal_high_s16(uv_bias, r_low, vCbR);
        cb_l_high = vmlal_high_s16(cb_l_high, g_low, vCbG);
        cb_l_high = vmlal_high_s16(cb_l_high, b_low, vCbB);

        uint32x4_t cb_l_low = vmlal_s16(uv_bias, r_l_low, vget_low_s16(vCbR));
        cb_l_low = vmlal_s16(cb_l_low, g_l_low, vget_low_s16(vCbG));
        cb_l_low = vmlal_s16(cb_l_low, b_l_low, vget_low_s16(vCbB));

        uint16x8_t cb_low = vminq_u16(
            vreinterpretq_u16_s16(vmaxq_s16(
                vcombine_s16(
                    vshrn_n_s32(cb_l_low, 8),
                    vshrn_n_s32(cb_l_high, 8)
                ),
                i_bias_y
            )),
            i_cap_uv
        );

        int32x4_t cr_l_high = vmlal_high_s16(uv_bias, r_low, vCrR);
        cr_l_high = vmlal_high_s16(cr_l_high, g_low, vCrG);
        cr_l_high = vmlal_high_s16(cr_l_high, b_low, vCrB);

        int32x4_t cr_l_low = vmlal_s16(uv_bias, r_l_low, vget_low_s16(vCrR));
        cr_l_low = vmlal_s16(cr_l_low, g_l_low, vget_low_s16(vCrG));
        cr_l_low = vmlal_s16(cr_l_low, b_l_low, vget_low_s16(vCrB));

        uint16x8_t cr_low = vminq_u16(
            vreinterpretq_u16_s16(vmaxq_s16(
                vcombine_s16(
                    vshrn_n_s32(cr_l_low, 8),
                    vshrn_n_s32(cr_l_high, 8)
                ),
                i_bias_y
            )),
            i_cap_uv
        );
        uint8x16_t cb = vcombine_u8(vqmovn_u16(cb_low), vqmovn_u16(cb_high));
        uint8x16_t cr = vcombine_u8(vqmovn_u16(cr_low), vqmovn_u16(cr_high));

        uint8x8_t cb_s = vrshrn_n_u16(vpaddlq_u8(cb), 1);
        uint8x8_t cr_s = vrshrn_n_u16(vpaddlq_u8(cr), 1);
        vst1_u8(uDst, cb_s);
        vst1_u8(vDst, cr_s);
      }

      uDst += 8;
      vDst += 8;

      yDst += 16;
      mSrc += 16 * 4;
    }
#endif

    for (; x < width; x += 2) {
      int r = mSrc[0];
      int g = mSrc[1];
      int b = mSrc[2];
      int y0 = ((r * YR + g * YG + b * YB + iBiasY) >> precision);
      yDst[0] = std::clamp(y0, static_cast<int>(biasY), maxLuma);

      yDst += 1;
      mSrc += 4;

      if (x + 1 < width) {
        int r1 = mSrc[0];
        int g1 = mSrc[1];
        int b1 = mSrc[2];
        int y1 = ((r1 * YR + g1 * YG + b1 * YB + iBiasY) >> precision);

        r = (r + r1 + 1) >> 1;
        g = (g + g1 + 1) >> 1;
        b = (b + b1 + 1) >> 1;

        yDst[0] = std::clamp(y1, static_cast<int>(0), maxLuma);
      }

      if ((y & 1) == 0) {
        int Cb = ((r * CbR + g * CbG + b * CbB + iBiasUV) >> precision);
        int Cr = ((r * CrR + g * CrG + b * CrB + iBiasUV) >> precision);
        uDst[0] = static_cast<uint8_t >(std::clamp(Cb, 0, maxChroma));
        vDst[0] = static_cast<uint8_t >(std::clamp(Cr, 0, maxChroma));
      }

      uDst += 1;
      vDst += 1;

      yDst += 1;
      mSrc += 4;
    }

    yStore += yStride;

    if (y & 1) {
      uStore += uStride;
      vStore += vStride;
    }

    mSource += sourceStride;
  }
}

void RgbaToYuv422(const uint8_t *sourceRgba, uint32_t sourceStride,
                  uint8_t *yPlane, uint32_t yStride,
                  uint8_t *uPlane, uint32_t uStride,
                  uint8_t *vPlane, uint32_t vStride,
                  uint32_t width, uint32_t height, YuvRange range, YuvMatrix matrix) {
  uint16_t biasY;
  uint16_t biasUV;
  uint16_t rangeY;
  uint16_t rangeUV;
  GetYUVRange(range, 8, biasY, biasUV, rangeY, rangeUV);

  int16_t YR, YG, YB;
  int16_t CbR, CbG, CbB;
  int16_t CrR, CrG, CrB;

  const int precision = 8;

  auto weights = getYuvWeights(matrix);

  ComputeTransformIntegers(weights.kr,
                           weights.kb,
                           static_cast<float>(biasY),
                           static_cast<float>(biasUV),
                           static_cast<float>(rangeY),
                           static_cast<float>(rangeUV),
                           255.f,
                           precision,
                           YR,
                           YG,
                           YB,
                           CbR,
                           CbG,
                           CbB,
                           CrR,
                           CrG,
                           CrB);

  const auto scale = static_cast<float>( 1 << precision );
  const auto iBiasY = static_cast<uint16_t>((static_cast<float>(biasY) + 0.5f) * scale);
  const auto iBiasUV = static_cast<uint16_t>((static_cast<float>(biasUV) + 0.5f) * scale);

  auto yStore = reinterpret_cast<uint8_t *>(yPlane);
  auto uStore = reinterpret_cast<uint8_t *>(uPlane);
  auto vStore = reinterpret_cast<uint8_t *>(vPlane);

  auto mSource = reinterpret_cast<const uint8_t *>(sourceRgba);

  int maxChroma = biasY + rangeUV;
  int maxLuma = biasY + rangeY;

  for (uint32_t y = 0; y < height; ++y) {

    auto yDst = reinterpret_cast<uint8_t *>(yStore);
    auto uDst = reinterpret_cast<uint8_t *>(uStore);
    auto vDst = reinterpret_cast<uint8_t *>(vStore);

    auto mSrc = reinterpret_cast<const uint8_t *>(mSource);

    uint32_t x = 0;

#if HAVE_NEON
    int16x8_t i_bias_y = vdupq_n_s16(static_cast<int16_t>(biasY));
    uint16x8_t i_cap_y = vdupq_n_u16(static_cast<uint16_t>(rangeY + biasY));
    int16x8_t i_cap_uv = vdupq_n_u16(static_cast<int16_t>(biasY + rangeUV));

    int16x8_t y_bias = vdupq_n_s32(iBiasY);
    int16x8_t uv_bias = vdupq_n_s32(iBiasUV);
    int16x8_t vYr = vdupq_n_s16(static_cast<int16_t>(YR));
    int16x8_t vYg = vdupq_n_s16(static_cast<int16_t>(YG));
    int16x8_t vYb = vdupq_n_s16(static_cast<int16_t>(YB));
    int16x8_t vCbR = vdupq_n_s16(static_cast<int16_t>(CbR));
    int16x8_t vCbG = vdupq_n_s16(static_cast<int16_t>(CbG));
    int16x8_t vCbB = vdupq_n_s16(static_cast<int16_t>(CbB));
    int16x8_t vCrR = vdupq_n_s16(static_cast<int16_t>(CrR));
    int16x8_t vCrG = vdupq_n_s16(static_cast<int16_t>(CrG));
    int16x8_t vCrB = vdupq_n_s16(static_cast<int16_t>(CrB));
    int32x4_t v_zeros = vdupq_n_s32(0);

    for (; x + 16 < width; x += 16) {
      uint8x16x4_t pixel = vld4q_u8(mSrc);

      int16x8_t r_high = vreinterpretq_s16_u16(vmovl_high_u8(pixel.val[0]));
      int16x8_t g_high = vreinterpretq_s16_u16(vmovl_high_u8(pixel.val[1]));
      int16x8_t b_high = vreinterpretq_s16_u16(vmovl_high_u8(pixel.val[2]));

      int16x4_t r_h_low = vget_low_s16(r_high);
      int16x4_t g_h_low = vget_low_s16(g_high);
      int16x4_t b_h_low = vget_low_s16(b_high);

      int32x4_t y_h_high = vmlal_high_s16(y_bias, r_high, vYr);
      y_h_high = vmlal_high_s16(y_h_high, g_high, vYg);
      y_h_high = vmlal_high_s16(y_h_high, b_high, vYb);
      y_h_high = vmaxq_s32(y_h_high, v_zeros);

      int32x4_t y_h_low = vmlal_s16(y_bias, r_h_low, vget_low_s16(vYr));
      y_h_low = vmlal_s16(y_h_low, g_h_low, vget_low_s16(vYg));
      y_h_low = vmlal_s16(y_h_low, b_h_low, vget_low_s16(vYb));
      y_h_low = vmaxq_s32(y_h_low, v_zeros);

      uint16x8_t y_high = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(y_h_low, 8),
                  vshrn_n_s32(y_h_high, 8)
              ),
              i_bias_y
          )),
          i_cap_y
      );

      int16x8_t r_low = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pixel.val[0])));
      int16x8_t g_low = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pixel.val[1])));
      int16x8_t b_low = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pixel.val[2])));

      int16x4_t r_l_low = vget_low_s16(r_low);
      int16x4_t g_l_low = vget_low_s16(g_low);
      int16x4_t b_l_low = vget_low_s16(b_low);

      int32x4_t y_l_high = vmlal_high_s16(y_bias, r_low, vYr);
      y_l_high = vmlal_high_s16(y_l_high, g_low, vYg);
      y_l_high = vmlal_high_s16(y_l_high, b_low, vYb);
      y_l_high = vmaxq_s32(y_l_high, v_zeros);

      int32x4_t y_l_low = vmlal_s16(y_bias, r_l_low, vget_low_s16(vYr));
      y_l_low = vmlal_s16(y_l_low, g_l_low, vget_low_s16(vYg));
      y_l_low = vmlal_s16(y_l_low, b_l_low, vget_low_s16(vYb));
      y_l_low = vmaxq_s32(y_l_low, v_zeros);

      uint16x8_t y_low = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(y_l_low, 8),
                  vshrn_n_s32(y_l_high, 8)
              ),
              i_bias_y
          )),
          i_cap_y
      );

      uint8x16_t yValues = vcombine_u8(vqmovn_u16(y_low), vqmovn_u16(y_high));
      vst1q_u8(yDst, yValues);

      int32x4_t cb_h_high = vmlal_high_s16(uv_bias, r_high, vCbR);
      cb_h_high = vmlal_high_s16(cb_h_high, g_high, vCbG);
      cb_h_high = vmlal_high_s16(cb_h_high, b_high, vCbB);

      int32x4_t cb_h_low = vmlal_s16(uv_bias, r_h_low, vget_low_s16(vCbR));
      cb_h_low = vmlal_s16(cb_h_low, g_h_low, vget_low_s16(vCbG));
      cb_h_low = vmlal_s16(cb_h_low, b_h_low, vget_low_s16(vCbB));

      uint16x8_t cb_high = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(cb_h_low, 8),
                  vshrn_n_s32(cb_h_high, 8)
              ),
              i_bias_y
          )),
          i_cap_uv
      );

      int32x4_t cr_h_high = vmlal_high_s16(uv_bias, r_high, vCrR);
      cr_h_high = vmlal_high_s16(cr_h_high, g_high, vCrG);
      cr_h_high = vmlal_high_s16(cr_h_high, b_high, vCrB);

      int32x4_t cr_h_low = vmlal_s16(uv_bias, r_h_low, vget_low_s16(vCrR));
      cr_h_low = vmlal_s16(cr_h_low, g_h_low, vget_low_s16(vCrG));
      cr_h_low = vmlal_s16(cr_h_low, b_h_low, vget_low_s16(vCrB));

      uint16x8_t cr_high = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(cr_h_low, 8),
                  vshrn_n_s32(cr_h_high, 8)
              ),
              i_bias_y
          )),
          i_cap_uv
      );

      uint32x4_t cb_l_high = vmlal_high_s16(uv_bias, r_low, vCbR);
      cb_l_high = vmlal_high_s16(cb_l_high, g_low, vCbG);
      cb_l_high = vmlal_high_s16(cb_l_high, b_low, vCbB);

      uint32x4_t cb_l_low = vmlal_s16(uv_bias, r_l_low, vget_low_s16(vCbR));
      cb_l_low = vmlal_s16(cb_l_low, g_l_low, vget_low_s16(vCbG));
      cb_l_low = vmlal_s16(cb_l_low, b_l_low, vget_low_s16(vCbB));

      uint16x8_t cb_low = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(cb_l_low, 8),
                  vshrn_n_s32(cb_l_high, 8)
              ),
              i_bias_y
          )),
          i_cap_uv
      );

      int32x4_t cr_l_high = vmlal_high_s16(uv_bias, r_low, vCrR);
      cr_l_high = vmlal_high_s16(cr_l_high, g_low, vCrG);
      cr_l_high = vmlal_high_s16(cr_l_high, b_low, vCrB);

      int32x4_t cr_l_low = vmlal_s16(uv_bias, r_l_low, vget_low_s16(vCrR));
      cr_l_low = vmlal_s16(cr_l_low, g_l_low, vget_low_s16(vCrG));
      cr_l_low = vmlal_s16(cr_l_low, b_l_low, vget_low_s16(vCrB));

      uint16x8_t cr_low = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(cr_l_low, 8),
                  vshrn_n_s32(cr_l_high, 8)
              ),
              i_bias_y
          )),
          i_cap_uv
      );
      uint8x16_t cb = vcombine_u8(vqmovn_u16(cb_low), vqmovn_u16(cb_high));
      uint8x16_t cr = vcombine_u8(vqmovn_u16(cr_low), vqmovn_u16(cr_high));

      uint8x8_t cb_s = vrshrn_n_u16(vpaddlq_u8(cb), 1);
      uint8x8_t cr_s = vrshrn_n_u16(vpaddlq_u8(cr), 1);
      vst1_u8(uDst, cb_s);
      vst1_u8(vDst, cr_s);

      uDst += 8;
      vDst += 8;

      yDst += 16;
      mSrc += 16 * 4;
    }
#endif

    for (; x < width; x += 2) {
      int r = mSrc[0];
      int g = mSrc[1];
      int b = mSrc[2];
      int y0 = ((r * YR + g * YG + b * YB + iBiasY) >> precision);
      yDst[0] = std::clamp(y0, static_cast<int>(biasY), maxLuma);

      yDst += 1;
      mSrc += 4;

      if (x + 1 < width) {
        int r1 = mSrc[0];
        int g1 = mSrc[1];
        int b1 = mSrc[2];
        int y1 = ((r1 * YR + g1 * YG + b1 * YB + iBiasY) >> precision);

        r = (r + r1 + 1) >> 1;
        g = (g + g1 + 1) >> 1;
        b = (b + b1 + 1) >> 1;

        yDst[0] = std::clamp(y1, static_cast<int>(biasY), maxLuma);
      }

      int Cb = ((r * CbR + g * CbG + b * CbB + iBiasUV) >> precision);
      int Cr = ((r * CrR + g * CrG + b * CrB + iBiasUV) >> precision);
      uDst[0] = static_cast<uint8_t >(std::clamp(Cb, 0, maxChroma));
      vDst[0] = static_cast<uint8_t >(std::clamp(Cr, 0, maxChroma));

      uDst += 1;
      vDst += 1;

      yDst += 1;
      mSrc += 4;
    }

    yStore += yStride;
    uStore += uStride;
    vStore += vStride;

    mSource += sourceStride;
  }
}

void RgbaToYuv444(const uint8_t *sourceRgba, uint32_t sourceStride,
                  uint8_t *yPlane, uint32_t yStride,
                  uint8_t *uPlane, uint32_t uStride,
                  uint8_t *vPlane, uint32_t vStride,
                  uint32_t width, uint32_t height, YuvRange range, YuvMatrix matrix) {
  uint16_t biasY;
  uint16_t biasUV;
  uint16_t rangeY;
  uint16_t rangeUV;
  GetYUVRange(range, 8, biasY, biasUV, rangeY, rangeUV);

  int16_t YR, YG, YB;
  int16_t CbR, CbG, CbB;
  int16_t CrR, CrG, CrB;

  const int precision = 8;

  auto weights = getYuvWeights(matrix);

  ComputeTransformIntegers(weights.kr,
                           weights.kb,
                           static_cast<float>(biasY),
                           static_cast<float>(biasUV),
                           static_cast<float>(rangeY),
                           static_cast<float>(rangeUV),
                           255.f,
                           precision,
                           YR,
                           YG,
                           YB,
                           CbR,
                           CbG,
                           CbB,
                           CrR,
                           CrG,
                           CrB);

  const auto scale = static_cast<float>( 1 << precision );
  const auto iBiasY = static_cast<uint16_t>((static_cast<float>(biasY) + 0.5f) * scale);
  const auto iBiasUV = static_cast<uint16_t>((static_cast<float>(biasUV) + 0.5f) * scale);

  int maxChroma = biasY + rangeUV;
  int maxLuma = biasY + rangeY;

  auto yStore = reinterpret_cast<uint8_t *>(yPlane);
  auto uStore = reinterpret_cast<uint8_t *>(uPlane);
  auto vStore = reinterpret_cast<uint8_t *>(vPlane);

  auto mSource = reinterpret_cast<const uint8_t *>(sourceRgba);

  for (uint32_t y = 0; y < height; ++y) {

    auto yDst = reinterpret_cast<uint8_t *>(yStore);
    auto uDst = reinterpret_cast<uint8_t *>(uStore);
    auto vDst = reinterpret_cast<uint8_t *>(vStore);

    auto mSrc = reinterpret_cast<const uint8_t *>(mSource);

    uint32_t x = 0;

#if HAVE_NEON
    int16x8_t i_bias_y = vdupq_n_s16(static_cast<int16_t>(biasY));
    uint16x8_t i_cap_y = vdupq_n_u16(static_cast<uint16_t>(rangeY + biasY));
    int16x8_t i_cap_uv = vdupq_n_u16(static_cast<int16_t>(biasY + rangeUV));

    int16x8_t y_bias = vdupq_n_s32(iBiasY);
    int16x8_t uv_bias = vdupq_n_s32(iBiasUV);
    int16x8_t vYr = vdupq_n_s16(static_cast<int16_t>(YR));
    int16x8_t vYg = vdupq_n_s16(static_cast<int16_t>(YG));
    int16x8_t vYb = vdupq_n_s16(static_cast<int16_t>(YB));
    int16x8_t vCbR = vdupq_n_s16(static_cast<int16_t>(CbR));
    int16x8_t vCbG = vdupq_n_s16(static_cast<int16_t>(CbG));
    int16x8_t vCbB = vdupq_n_s16(static_cast<int16_t>(CbB));
    int16x8_t vCrR = vdupq_n_s16(static_cast<int16_t>(CrR));
    int16x8_t vCrG = vdupq_n_s16(static_cast<int16_t>(CrG));
    int16x8_t vCrB = vdupq_n_s16(static_cast<int16_t>(CrB));
    int32x4_t v_zeros = vdupq_n_s32(0);

    for (; x + 16 < width; x += 16) {
      uint8x16x4_t pixel = vld4q_u8(mSrc);

      int16x8_t r_high = vreinterpretq_s16_u16(vmovl_high_u8(pixel.val[0]));
      int16x8_t g_high = vreinterpretq_s16_u16(vmovl_high_u8(pixel.val[1]));
      int16x8_t b_high = vreinterpretq_s16_u16(vmovl_high_u8(pixel.val[2]));

      int16x4_t r_h_low = vget_low_s16(r_high);
      int16x4_t g_h_low = vget_low_s16(g_high);
      int16x4_t b_h_low = vget_low_s16(b_high);

      int32x4_t y_h_high = vmlal_high_s16(y_bias, r_high, vYr);
      y_h_high = vmlal_high_s16(y_h_high, g_high, vYg);
      y_h_high = vmlal_high_s16(y_h_high, b_high, vYb);
      y_h_high = vmaxq_s32(y_h_high, v_zeros);

      int32x4_t y_h_low = vmlal_s16(y_bias, r_h_low, vget_low_s16(vYr));
      y_h_low = vmlal_s16(y_h_low, g_h_low, vget_low_s16(vYg));
      y_h_low = vmlal_s16(y_h_low, b_h_low, vget_low_s16(vYb));
      y_h_low = vmaxq_s32(y_h_low, v_zeros);

      uint16x8_t y_high = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(y_h_low, 8),
                  vshrn_n_s32(y_h_high, 8)
              ),
              i_bias_y
          )),
          i_cap_y
      );

      int16x8_t r_low = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pixel.val[0])));
      int16x8_t g_low = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pixel.val[1])));
      int16x8_t b_low = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pixel.val[2])));

      int16x4_t r_l_low = vget_low_s16(r_low);
      int16x4_t g_l_low = vget_low_s16(g_low);
      int16x4_t b_l_low = vget_low_s16(b_low);

      int32x4_t y_l_high = vmlal_high_s16(y_bias, r_low, vYr);
      y_l_high = vmlal_high_s16(y_l_high, g_low, vYg);
      y_l_high = vmlal_high_s16(y_l_high, b_low, vYb);
      y_l_high = vmaxq_s32(y_l_high, v_zeros);

      int32x4_t y_l_low = vmlal_s16(y_bias, r_l_low, vget_low_s16(vYr));
      y_l_low = vmlal_s16(y_l_low, g_l_low, vget_low_s16(vYg));
      y_l_low = vmlal_s16(y_l_low, b_l_low, vget_low_s16(vYb));
      y_l_low = vmaxq_s32(y_l_low, v_zeros);

      uint16x8_t y_low = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(y_l_low, 8),
                  vshrn_n_s32(y_l_high, 8)
              ),
              i_bias_y
          )),
          i_cap_y
      );

      uint8x16_t yValues = vcombine_u8(vqmovn_u16(y_low), vqmovn_u16(y_high));
      vst1q_u8(yDst, yValues);

      int32x4_t cb_h_high = vmlal_high_s16(uv_bias, r_high, vCbR);
      cb_h_high = vmlal_high_s16(cb_h_high, g_high, vCbG);
      cb_h_high = vmlal_high_s16(cb_h_high, b_high, vCbB);

      int32x4_t cb_h_low = vmlal_s16(uv_bias, r_h_low, vget_low_s16(vCbR));
      cb_h_low = vmlal_s16(cb_h_low, g_h_low, vget_low_s16(vCbG));
      cb_h_low = vmlal_s16(cb_h_low, b_h_low, vget_low_s16(vCbB));

      uint16x8_t cb_high = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(cb_h_low, 8),
                  vshrn_n_s32(cb_h_high, 8)
              ),
              i_bias_y
          )),
          i_cap_uv
      );

      int32x4_t cr_h_high = vmlal_high_s16(uv_bias, r_high, vCrR);
      cr_h_high = vmlal_high_s16(cr_h_high, g_high, vCrG);
      cr_h_high = vmlal_high_s16(cr_h_high, b_high, vCrB);

      int32x4_t cr_h_low = vmlal_s16(uv_bias, r_h_low, vget_low_s16(vCrR));
      cr_h_low = vmlal_s16(cr_h_low, g_h_low, vget_low_s16(vCrG));
      cr_h_low = vmlal_s16(cr_h_low, b_h_low, vget_low_s16(vCrB));

      uint16x8_t cr_high = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(cr_h_low, 8),
                  vshrn_n_s32(cr_h_high, 8)
              ),
              i_bias_y
          )),
          i_cap_uv
      );

      uint32x4_t cb_l_high = vmlal_high_s16(uv_bias, r_low, vCbR);
      cb_l_high = vmlal_high_s16(cb_l_high, g_low, vCbG);
      cb_l_high = vmlal_high_s16(cb_l_high, b_low, vCbB);

      uint32x4_t cb_l_low = vmlal_s16(uv_bias, r_l_low, vget_low_s16(vCbR));
      cb_l_low = vmlal_s16(cb_l_low, g_l_low, vget_low_s16(vCbG));
      cb_l_low = vmlal_s16(cb_l_low, b_l_low, vget_low_s16(vCbB));

      uint16x8_t cb_low = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(cb_l_low, 8),
                  vshrn_n_s32(cb_l_high, 8)
              ),
              i_bias_y
          )),
          i_cap_uv
      );

      int32x4_t cr_l_high = vmlal_high_s16(uv_bias, r_low, vCrR);
      cr_l_high = vmlal_high_s16(cr_l_high, g_low, vCrG);
      cr_l_high = vmlal_high_s16(cr_l_high, b_low, vCrB);

      int32x4_t cr_l_low = vmlal_s16(uv_bias, r_l_low, vget_low_s16(vCrR));
      cr_l_low = vmlal_s16(cr_l_low, g_l_low, vget_low_s16(vCrG));
      cr_l_low = vmlal_s16(cr_l_low, b_l_low, vget_low_s16(vCrB));

      uint16x8_t cr_low = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(cr_l_low, 8),
                  vshrn_n_s32(cr_l_high, 8)
              ),
              i_bias_y
          )),
          i_cap_uv
      );
      uint8x16_t cb = vcombine_u8(vqmovn_u16(cb_low), vqmovn_u16(cb_high));
      uint8x16_t cr = vcombine_u8(vqmovn_u16(cr_low), vqmovn_u16(cr_high));

      vst1q_u8(uDst, cb);
      vst1q_u8(vDst, cr);

      uDst += 16;
      vDst += 16;

      yDst += 16;
      mSrc += 16 * 4;
    }
#endif

    for (; x < width; ++x) {
      int r = mSrc[0];
      int g = mSrc[1];
      int b = mSrc[2];
      int y0 = ((r * YR + g * YG + b * YB + iBiasY) >> precision);
      yDst[0] = std::clamp(y0, static_cast<int>(biasY), maxLuma);

      int Cb = ((r * static_cast<int>(CbR) + g * static_cast<int>(CbG) + b * static_cast<int>(CbB)
          + iBiasUV) >> precision);
      int Cr = ((r * CrR + g * CrG + b * CrB + iBiasUV) >> precision);
      uDst[0] = static_cast<uint8_t >(std::clamp(Cb, 0, maxChroma));
      vDst[0] = static_cast<uint8_t >(std::clamp(Cr, 0, maxChroma));

      uDst += 1;
      vDst += 1;
      yDst += 1;
      mSrc += 4;
    }

    yStore += yStride;
    uStore += uStride;
    vStore += vStride;

    mSource += sourceStride;
  }
}

void RgbaToYuv400(const uint8_t *sourceRgba, uint32_t sourceStride,
                  uint8_t *yPlane, uint32_t yStride,
                  uint32_t width, uint32_t height, YuvRange range, YuvMatrix matrix) {
  uint16_t biasY;
  uint16_t biasUV;
  uint16_t rangeY;
  uint16_t rangeUV;
  GetYUVRange(range, 8, biasY, biasUV, rangeY, rangeUV);

  int16_t YR, YG, YB;
  int16_t CbR, CbG, CbB;
  int16_t CrR, CrG, CrB;

  const int precision = 8;

  auto weights = getYuvWeights(matrix);

  ComputeTransformIntegers(weights.kr,
                           weights.kb,
                           static_cast<float>(biasY),
                           static_cast<float>(biasUV),
                           static_cast<float>(rangeY),
                           static_cast<float>(rangeUV),
                           255.f,
                           precision,
                           YR,
                           YG,
                           YB,
                           CbR,
                           CbG,
                           CbB,
                           CrR,
                           CrG,
                           CrB);

  const auto scale = static_cast<float>( 1 << precision );
  const auto iBiasY = static_cast<uint16_t>((static_cast<float>(biasY) + 0.5f) * scale);

  auto yStore = reinterpret_cast<uint8_t *>(yPlane);

  auto mSource = reinterpret_cast<const uint8_t *>(sourceRgba);

  int maxLuma = biasY + rangeY;

  for (uint32_t y = 0; y < height; ++y) {

    auto yDst = reinterpret_cast<uint8_t *>(yStore);

    auto mSrc = reinterpret_cast<const uint8_t *>(mSource);

    uint32_t x = 0;

#if HAVE_NEON
    int16x8_t i_bias_y = vdupq_n_s16(static_cast<int16_t>(biasY));
    uint16x8_t i_cap_y = vdupq_n_u16(static_cast<uint16_t>(rangeY + biasY));
    int16x8_t i_cap_uv = vdupq_n_u16(static_cast<int16_t>(biasY + rangeUV));

    int16x8_t y_bias = vdupq_n_s32(iBiasY);
    int16x8_t vYr = vdupq_n_s16(static_cast<int16_t>(YR));
    int16x8_t vYg = vdupq_n_s16(static_cast<int16_t>(YG));
    int16x8_t vYb = vdupq_n_s16(static_cast<int16_t>(YB));
    int32x4_t v_zeros = vdupq_n_s32(0);

    for (; x + 16 < width; x += 16) {
      uint8x16x4_t pixel = vld4q_u8(mSrc);

      int16x8_t r_high = vreinterpretq_s16_u16(vmovl_high_u8(pixel.val[0]));
      int16x8_t g_high = vreinterpretq_s16_u16(vmovl_high_u8(pixel.val[1]));
      int16x8_t b_high = vreinterpretq_s16_u16(vmovl_high_u8(pixel.val[2]));

      int16x4_t r_h_low = vget_low_s16(r_high);
      int16x4_t g_h_low = vget_low_s16(g_high);
      int16x4_t b_h_low = vget_low_s16(b_high);

      int32x4_t y_h_high = vmlal_high_s16(y_bias, r_high, vYr);
      y_h_high = vmlal_high_s16(y_h_high, g_high, vYg);
      y_h_high = vmlal_high_s16(y_h_high, b_high, vYb);
      y_h_high = vmaxq_s32(y_h_high, v_zeros);

      int32x4_t y_h_low = vmlal_s16(y_bias, r_h_low, vget_low_s16(vYr));
      y_h_low = vmlal_s16(y_h_low, g_h_low, vget_low_s16(vYg));
      y_h_low = vmlal_s16(y_h_low, b_h_low, vget_low_s16(vYb));
      y_h_low = vmaxq_s32(y_h_low, v_zeros);

      uint16x8_t y_high = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(y_h_low, 8),
                  vshrn_n_s32(y_h_high, 8)
              ),
              i_bias_y
          )),
          i_cap_y
      );

      int16x8_t r_low = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pixel.val[0])));
      int16x8_t g_low = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pixel.val[1])));
      int16x8_t b_low = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pixel.val[2])));

      int16x4_t r_l_low = vget_low_s16(r_low);
      int16x4_t g_l_low = vget_low_s16(g_low);
      int16x4_t b_l_low = vget_low_s16(b_low);

      int32x4_t y_l_high = vmlal_high_s16(y_bias, r_low, vYr);
      y_l_high = vmlal_high_s16(y_l_high, g_low, vYg);
      y_l_high = vmlal_high_s16(y_l_high, b_low, vYb);
      y_l_high = vmaxq_s32(y_l_high, v_zeros);

      int32x4_t y_l_low = vmlal_s16(y_bias, r_l_low, vget_low_s16(vYr));
      y_l_low = vmlal_s16(y_l_low, g_l_low, vget_low_s16(vYg));
      y_l_low = vmlal_s16(y_l_low, b_l_low, vget_low_s16(vYb));
      y_l_low = vmaxq_s32(y_l_low, v_zeros);

      uint16x8_t y_low = vminq_u16(
          vreinterpretq_u16_s16(vmaxq_s16(
              vcombine_s16(
                  vshrn_n_s32(y_l_low, 8),
                  vshrn_n_s32(y_l_high, 8)
              ),
              i_bias_y
          )),
          i_cap_y
      );

      uint8x16_t yValues = vcombine_u8(vqmovn_u16(y_low), vqmovn_u16(y_high));
      vst1q_u8(yDst, yValues);
      yDst += 16;
      mSrc += 16 * 4;
    }
#endif

    for (; x < width; x += 2) {
      int r = mSrc[0];
      int g = mSrc[1];
      int b = mSrc[2];
      int y0 = ((r * YR + g * YG + b * YB + iBiasY) >> precision);
      yDst[0] = std::clamp(y0, static_cast<int>(biasY), maxLuma);

      yDst += 1;
      mSrc += 4;

      if (x + 1 < width) {
        int r1 = mSrc[0];
        int g1 = mSrc[1];
        int b1 = mSrc[2];
        int y1 = ((r1 * YR + g1 * YG + b1 * YB + iBiasY) >> precision);

        yDst[0] = std::clamp(y1, static_cast<int>(0), maxLuma);
      }

      yDst += 1;
      mSrc += 4;
    }

    yStore += yStride;
    mSource += sourceStride;
  }
}