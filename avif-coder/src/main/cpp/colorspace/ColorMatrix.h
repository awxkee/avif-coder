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
