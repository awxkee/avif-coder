/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 10/11/2024
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


#include "FilmicToneMapper.h"
#include "Oklab.hpp"

void FilmicToneMapper::transferTone(float *inPlace, uint32_t width) {
  float *targetPlace = inPlace;

  for (uint32_t x = 0; x < width; ++x) {
    float r = targetPlace[0];
    float g = targetPlace[1];
    float b = targetPlace[2];
    coder::Oklab oklab = coder::Oklab::fromLinearRGB(r, g, b);
    if (oklab.L == 0) {
      continue;
    }
    float shScale = this->uncharted2_filmic(oklab.L) / oklab.L;
    oklab.L = oklab.L * shScale;
    coder::Rgb linearRgb = oklab.toLinearRGB();
    targetPlace[0] = std::min(linearRgb.r, 1.f);
    targetPlace[1] = std::min(linearRgb.g, 1.f);
    targetPlace[2] = std::min(linearRgb.b, 1.f);
    targetPlace += 3;
  }
}