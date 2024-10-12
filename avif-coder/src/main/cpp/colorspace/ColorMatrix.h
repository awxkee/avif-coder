//
// Created by Radzivon Bartoshyk on 11/10/2024.
//

#ifndef AVIF_COLORMATRIX_H
#define AVIF_COLORMATRIX_H

#include <vector>
#include <cstdint>
#include "Trc.h"
#include "ToneMapper.h"
#include "ITUR.h"

void applyColorMatrix(uint8_t *inPlace, uint32_t stride, uint32_t width, uint32_t height,
                      const float *matrix, TransferFunction intoLinear, TransferFunction intoGamma,
                      CurveToneMapper toneMapper, ITURColorCoefficients coeffs);

void applyColorMatrix16Bit(uint16_t *inPlace,
                           uint32_t stride,
                           uint32_t width,
                           uint32_t height,
                           uint8_t bitDepth,
                           const float *matrix,
                           TransferFunction intoLinear,
                           TransferFunction intoGamma,
                           CurveToneMapper toneMapper,
                           ITURColorCoefficients coeffs);

#endif //AVIF_COLORMATRIX_H
