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

  CbR = 0.5f * kr / (1.f - kb) * rangeUV / fullRangeRGB;
  CbG = 0.5f * kg / (1.f - kb) * rangeUV / fullRangeRGB;
  CbB = 0.5f * rangeUV / fullRangeRGB;

  CrR = 0.5f * rangeUV / fullRangeRGB;
  CrG = 0.5f * kg / (1.f - kr) * rangeUV / fullRangeRGB;
  CrB = 0.5f * kb / (1.f - kr) * rangeUV / fullRangeRGB;
}

void ComputeTransformIntegers(const float kr,
                              const float kb,
                              const float biasY,
                              const float biasUV,
                              const float rangeY,
                              const float rangeUV,
                              const float fullRangeRGB,
                              const uint16_t precision,
                              uint16_t &uYR, uint16_t &uYG, uint16_t &uYB,
                              uint16_t &uCbR, uint16_t &uCbG, uint16_t &uCbB,
                              uint16_t &uCrR, uint16_t &uCrG, uint16_t &uCrB) {
  float YR, YG, YB;
  float CbR, CbG, CbB;
  float CrR, CrG, CrB;

  const auto scale = static_cast<float>( 1 << precision );

  ComputeTransform(kr, kb, static_cast<float>(biasY), static_cast<float>(biasUV),
                   static_cast<float>(rangeY), static_cast<float>(rangeUV),
                   fullRangeRGB, YR, YG, YB, CbR, CbG, CbB, CrR, CrG, CrB);

  uYR = static_cast<uint16_t>(::roundf(scale * YR));
  uYG = static_cast<uint16_t>(::roundf(scale * YG));
  uYB = static_cast<uint16_t>(::roundf(scale * YB));

  uCbR = static_cast<uint16_t>(::roundf(scale * CbR));
  uCbG = static_cast<uint16_t>(::roundf(scale * CbG));
  uCbB = static_cast<uint16_t>(::roundf(scale * CbB));

  uCrR = static_cast<uint16_t>(::roundf(scale * CrR));
  uCrG = static_cast<uint16_t>(::roundf(scale * CrG));
  uCrB = static_cast<uint16_t>(::roundf(scale * CrB));
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

  uint16_t YR, YG, YB;
  uint16_t CbR, CbG, CbB;
  uint16_t CrR, CrG, CrB;

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

  for (uint32_t y = 0; y < height; ++y) {

    auto yDst = reinterpret_cast<uint8_t *>(yStore);
    auto uDst = reinterpret_cast<uint8_t *>(uStore);
    auto vDst = reinterpret_cast<uint8_t *>(vStore);

    auto mSrc = reinterpret_cast<const uint8_t *>(mSource);

    for (uint32_t x = 0; x < width; x += 2) {
      int r = mSrc[0];
      int g = mSrc[1];
      int b = mSrc[2];
      int y0 = ((r * YR + g * YG + b * YB + iBiasY) >> precision);
      yDst[0] = y0;

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

        yDst[0] = y1;
      }

      if ((y & 1) == 0) {
        int Cb = ((-r * CbR - g * CbG + b * CbB + iBiasUV) >> precision);
        int Cr = ((r * CrR - g * CrG - b * CrB + iBiasUV) >> precision);
        uDst[0] = static_cast<uint8_t >(Cb);
        vDst[0] = static_cast<uint8_t >(Cr);
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

  uint16_t YR, YG, YB;
  uint16_t CbR, CbG, CbB;
  uint16_t CrR, CrG, CrB;

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

  for (uint32_t y = 0; y < height; ++y) {

    auto yDst = reinterpret_cast<uint8_t *>(yStore);
    auto uDst = reinterpret_cast<uint8_t *>(uStore);
    auto vDst = reinterpret_cast<uint8_t *>(vStore);

    auto mSrc = reinterpret_cast<const uint8_t *>(mSource);

    for (uint32_t x = 0; x < width; x += 2) {
      int r = mSrc[0];
      int g = mSrc[1];
      int b = mSrc[2];
      int y0 = ((r * YR + g * YG + b * YB + iBiasY) >> precision);
      yDst[0] = y0;

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

        yDst[0] = y1;
      }

      int Cb = ((-r * CbR - g * CbG + b * CbB + iBiasUV) >> precision);
      int Cr = ((r * CrR - g * CrG - b * CrB + iBiasUV) >> precision);
      uDst[0] = static_cast<uint8_t >(Cb);
      vDst[0] = static_cast<uint8_t >(Cr);

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

  uint16_t YR, YG, YB;
  uint16_t CbR, CbG, CbB;
  uint16_t CrR, CrG, CrB;

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

  for (uint32_t y = 0; y < height; ++y) {

    auto yDst = reinterpret_cast<uint8_t *>(yStore);
    auto uDst = reinterpret_cast<uint8_t *>(uStore);
    auto vDst = reinterpret_cast<uint8_t *>(vStore);

    auto mSrc = reinterpret_cast<const uint8_t *>(mSource);

    for (uint32_t x = 0; x < width; ++x) {
      int r = mSrc[0];
      int g = mSrc[1];
      int b = mSrc[2];
      int y0 = ((r * YR + g * YG + b * YB + iBiasY) >> precision);
      yDst[0] = y0;

      int Cb = ((-r * static_cast<int>(CbR) - g * static_cast<int>(CbG) + b * static_cast<int>(CbB)
          + iBiasUV) >> precision);
      int Cr = ((r * CrR - g * CrG - b * CrB + iBiasUV) >> precision);
      *uDst = static_cast<uint8_t >(Cb);
      *vDst = static_cast<uint8_t >(Cr);

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