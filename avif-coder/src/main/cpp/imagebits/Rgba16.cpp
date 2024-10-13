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

#include "Rgba16.h"

namespace coder {
void
Rgba16ToRgba8(const uint16_t *source,
              uint32_t srcStride,
              uint8_t *destination,
              uint32_t dstStride,
              uint32_t width,
              uint32_t height,
              uint32_t bitDepth) {

  int diff = static_cast<int>(bitDepth) - 8;

  for (uint32_t y = 0; y < height; ++y) {
    auto data = reinterpret_cast<const uint16_t *>(reinterpret_cast<const uint8_t *>(source)
        + y * srcStride);
    auto dst =
        reinterpret_cast<uint8_t *>(reinterpret_cast<uint8_t *>(destination) + y * dstStride);
    for (uint32_t x = 0; x < width; ++x) {
      uint16_t r = data[0];
      uint16_t g = data[1];
      uint16_t b = data[2];
      uint16_t alpha = data[3];

      auto A16 = (uint32_t) alpha >> diff;
      auto R16 = (uint32_t) r >> diff;
      auto G16 = (uint32_t) g >> diff;
      auto B16 = (uint32_t) b >> diff;

      dst[0] = R16;
      dst[1] = G16;
      dst[2] = B16;
      dst[3] = A16;

      data += 4;
      dst += 4;
    }
  }
}
}