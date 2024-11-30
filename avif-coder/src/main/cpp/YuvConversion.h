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

#ifndef AVIF_CODER_SRC_MAIN_CPP_YUVCONVERSION_H_
#define AVIF_CODER_SRC_MAIN_CPP_YUVCONVERSION_H_

#include <cstdint>
#include "avifweaver.h"

void RgbaToYuv420(const uint8_t *sourceRgba, uint32_t sourceStride,
                  uint8_t *yPlane, uint32_t yStride,
                  uint8_t *uPlane, uint32_t uStride,
                  uint8_t *vPlane, uint32_t vStride,
                  uint32_t width, uint32_t height, YuvRange range, YuvMatrix matrix);

void RgbaToYuv422(const uint8_t *sourceRgba, uint32_t sourceStride,
                  uint8_t *yPlane, uint32_t yStride,
                  uint8_t *uPlane, uint32_t uStride,
                  uint8_t *vPlane, uint32_t vStride,
                  uint32_t width, uint32_t height, YuvRange range, YuvMatrix matrix);

void RgbaToYuv444(const uint8_t *sourceRgba, uint32_t sourceStride,
                  uint8_t *yPlane, uint32_t yStride,
                  uint8_t *uPlane, uint32_t uStride,
                  uint8_t *vPlane, uint32_t vStride,
                  uint32_t width, uint32_t height, YuvRange range, YuvMatrix matrix);

void RgbaToYuv400(const uint8_t *sourceRgba, uint32_t sourceStride,
                  uint8_t *yPlane, uint32_t yStride,
                  uint32_t width, uint32_t height, YuvRange range, YuvMatrix matrix);

#endif //AVIF_CODER_SRC_MAIN_CPP_YUVCONVERSION_H_
