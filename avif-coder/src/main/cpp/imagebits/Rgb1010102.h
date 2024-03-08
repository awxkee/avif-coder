/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 05/09/2023
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

#ifndef AVIF_RGB1010102_H
#define AVIF_RGB1010102_H

#include <vector>

namespace coder {

template<typename V>
void RGBA1010102ToUnsigned(const uint8_t *__restrict__ src, const int srcStride,
                           V *__restrict__ dst, const int dstStride,
                           const int width, const int height, const int bitDepth);

void
F16ToRGBA1010102(const uint16_t *source, int srcStride, uint8_t *destination, int dstStride,
                 int width,
                 int height);

void
Rgba8ToRGBA1010102(const uint8_t *source,
                   int srcStride,
                   uint8_t *destination,
                   int dstStride,
                   int width,
                   int height,
                   const bool attenuateAlpha);
}

#endif //AVIF_RGB1010102_H
