/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 12/11/2024
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

#include "AcesToneMapper.h"
#include "Oklab.hpp"
#include <algorithm>

void AcesToneMapper::transferTone(float *inPlace, uint32_t width) {
  float *targetPlace = inPlace;

  for (uint32_t x = 0; x < width; ++x) {
    float r = targetPlace[0];
    float g = targetPlace[1];
    float b = targetPlace[2];

    auto mulInput = [](const coder::Rgb &color) {
      float a = 0.59719f * color.r + 0.35458f * color.g + 0.04823f * color.b,
          b = 0.07600f * color.r + 0.90834f * color.g + 0.01566f * color.b,
          c = 0.02840f * color.r + 0.13383f * color.g + 0.83777f * color.b;
      return coder::Rgb(a, b, c);
    };

    auto mulOutput = [](const coder::Rgb &color) {
      float a = 1.60475f * color.r - 0.53108f * color.g - 0.07367f * color.b,
          b = -0.10208f * color.r + 1.10813f * color.g - 0.00605f * color.b,
          c = -0.00327f * color.r - 0.07276f * color.g + 1.07602f * color.b;
      return coder::Rgb(a, b, c);
    };

    auto colorIn = mulInput(coder::Rgb(r, g, b));
    auto ca = colorIn * (colorIn + 0.0245786f) - 0.000090537f;
    auto cb = colorIn * (0.983729f * colorIn + 0.4329510f) + 0.238081f;
    auto cOut = mulOutput(ca / cb);

    targetPlace[0] = std::min(cOut.r, 1.f);
    targetPlace[1] = std::min(cOut.g, 1.f);
    targetPlace[2] = std::min(cOut.b, 1.f);

    targetPlace += 3;
  }
}