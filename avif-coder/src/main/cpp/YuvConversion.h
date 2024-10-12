//
// Created by Radzivon Bartoshyk on 12/10/2024.
//

#ifndef AVIF_AVIF_CODER_SRC_MAIN_CPP_YUVCONVERSION_H_
#define AVIF_AVIF_CODER_SRC_MAIN_CPP_YUVCONVERSION_H_

#include <cstdint>

enum YuvMatrix {
  Bt601,
  Bt709,
  Bt2020
};

struct YuvMatrixWeights {
  float kr;
  float kb;
};

enum YuvRange {
  Tv, Full
};

YuvMatrixWeights getYuvWeights(YuvMatrix matrix);

void GetYUVRange(YuvRange yuvColorRange, int bitDepth,
                 uint16_t &biasY, uint16_t &biasUV,
                 uint16_t &rangeY, uint16_t &rangeUV);

void ComputeForwardIntegersTransform(float kr,
                                     float kb,
                                     float biasY,
                                     float biasUV,
                                     float rangeY,
                                     float rangeUV,
                                     float fullRangeRGB,
                                     uint16_t precision,
                                     uint16_t uvPrecision,
                                     uint16_t &uYR, uint16_t &uYG, uint16_t &uYB,
                                     uint16_t &kuCb, uint16_t &kuCr);

void ComputeTransform(float kr,
                      float kb,
                      float biasY,
                      float biasUV,
                      float rangeY,
                      float rangeUV,
                      float fullRangeRGB,
                      float &YR, float &YG, float &YB,
                      float &CbR, float &CbG, float &CbB,
                      float &CrR, float &CrG, float &CrB);

void ComputeTransformIntegers(float kr,
                              float kb,
                              float biasY,
                              float biasUV,
                              float rangeY,
                              float rangeUV,
                              float fullRangeRGB,
                              uint16_t precision,
                              uint16_t &uYR, uint16_t &uYG, uint16_t &uYB,
                              uint16_t &uCbR, uint16_t &uCbG, uint16_t &uCbB,
                              uint16_t &uCrR, uint16_t &uCrG, uint16_t &uCrB);

void RgbaToYuv420(uint8_t *sourceRgba, uint32_t sourceStride,
                  uint8_t *yPlane, uint32_t yStride,
                  uint8_t *uPlane, uint32_t uStride,
                  uint8_t *vPlane, uint32_t vStride,
                  uint32_t width, uint32_t height, YuvRange range, YuvMatrix matrix);

void RgbaToYuv422(uint8_t *sourceRgba, uint32_t sourceStride,
                  uint8_t *yPlane, uint32_t yStride,
                  uint8_t *uPlane, uint32_t uStride,
                  uint8_t *vPlane, uint32_t vStride,
                  uint32_t width, uint32_t height, YuvRange range, YuvMatrix matrix);

void RgbaToYuv444(uint8_t *sourceRgba, uint32_t sourceStride,
                  uint8_t *yPlane, uint32_t yStride,
                  uint8_t *uPlane, uint32_t uStride,
                  uint8_t *vPlane, uint32_t vStride,
                  uint32_t width, uint32_t height, YuvRange range, YuvMatrix matrix);

#endif //AVIF_AVIF_CODER_SRC_MAIN_CPP_YUVCONVERSION_H_
