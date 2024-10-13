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

#include "ScanAlpha.h"
#include <limits>

template<typename T>
bool isImageHasAlpha(T *image, uint32_t stride, uint32_t width, uint32_t height) {
  T firstItem = image[3];
  if (firstItem != std::numeric_limits<T>::max()) {
    return true;
  }
  for (uint32_t y = 0; y < height; ++y) {
    uint32_t sums = 0;
    auto lane = reinterpret_cast<T *>(reinterpret_cast<uint8_t *>(image) + y * stride);
    for (uint32_t x = 0; x < width; ++x) {
      sums += lane[3] ^ firstItem;
      lane += 4;
    }
    if (sums != 0) {
      return true;
    }
  }

  return false;
}

template bool isImageHasAlpha(uint8_t *image, uint32_t stride, uint32_t width, uint32_t height);
template bool isImageHasAlpha(uint16_t *image, uint32_t stride, uint32_t width, uint32_t height);