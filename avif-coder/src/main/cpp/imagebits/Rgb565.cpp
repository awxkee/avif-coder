/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 15/09/2023
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

#include "Rgb565.h"
#include <thread>
#include <vector>
#include "half.hpp"
#include <algorithm>
#include "concurrency.hpp"

using namespace std;

namespace coder {

void Rgb565ToUnsigned8(const uint16_t *sourceData, uint32_t srcStride,
                       uint8_t *destination, uint32_t dstStride, uint32_t width,
                       uint32_t height, const uint8_t bgColor) {
  for (uint32_t y = 0; y < height; ++y) {
    auto src = reinterpret_cast<const uint16_t *>(reinterpret_cast<const uint8_t *>(sourceData)
        + y * srcStride);
    auto dst = reinterpret_cast<uint8_t *>(destination) + y * dstStride;
    for (uint32_t x = 0; x < width; ++x) {
      uint16_t color565 = src[0];

      uint16_t red8 = (color565 & 0b1111100000000000) >> 8;
      uint16_t green8 = (color565 & 0b11111100000) >> 3;
      uint16_t blue8 = (color565 & 0b11111) << 3;

      dst[0] = static_cast<uint8_t>(red8);
      dst[1] = static_cast<uint8_t>(green8);
      dst[2] = static_cast<uint8_t>(blue8);
      dst[3] = bgColor;

      src += 1;
      dst += 4;
    }
  }
}

void Rgba8To565(const uint8_t *sourceData, int srcStride,
                uint16_t *destination, int dstStride, int width,
                int height, const bool attenuateAlpha) {
  for (uint32_t y = 0; y < height; ++y) {
    auto src = reinterpret_cast<const uint8_t *>(reinterpret_cast<const uint8_t *>(sourceData)
        + y * srcStride);
    auto dst =
        reinterpret_cast<uint16_t *>( reinterpret_cast<uint8_t *>(destination) + y * dstStride);
    for (uint32_t x = 0; x < width; ++x) {
      uint8_t alpha = src[3];
      uint8_t r = src[0];
      uint8_t g = src[1];
      uint8_t b = src[2];

      if (attenuateAlpha) {
        r = (static_cast<uint16_t>(r) * static_cast<uint16_t>(alpha)) / static_cast<uint16_t >(255);
        g = (static_cast<uint16_t>(g) * static_cast<uint16_t>(alpha)) / static_cast<uint16_t >(255);
        b = (static_cast<uint16_t>(b) * static_cast<uint16_t>(alpha)) / static_cast<uint16_t >(255);
      }

      uint16_t red565 = (r >> 3) << 11;
      uint16_t green565 = (g >> 2) << 5;
      uint16_t blue565 = b >> 3;

      auto result = static_cast<uint16_t>(red565 | green565 | blue565);
      dst[0] = result;

      src += 4;
      dst += 1;
    }
  }
}

void Rgba16To565(const uint16_t *sourceData, uint32_t srcStride,
                 uint16_t *destination, uint32_t dstStride, uint32_t width,
                 uint32_t height, uint32_t bitDepth) {

  uint32_t greenDiff = bitDepth - 8 + 2;
  uint32_t redBlueDiff = bitDepth - 8 + 3;

  for (uint32_t y = 0; y < height; ++y) {
    auto src = reinterpret_cast<const uint16_t *>(reinterpret_cast<const uint8_t *>(sourceData)
        + y * srcStride);
    auto dst =
        reinterpret_cast<uint16_t *>( reinterpret_cast<uint8_t *>(destination) + y * dstStride);
    for (uint32_t x = 0; x < width; ++x) {
      uint16_t r = src[0];
      uint16_t g = src[1];
      uint16_t b = src[2];

      uint16_t red565 = (r >> redBlueDiff) << 11;
      uint16_t green565 = (g >> greenDiff) << 5;
      uint16_t blue565 = b >> redBlueDiff;

      auto result = static_cast<uint16_t>(red565 | green565 | blue565);
      dst[0] = result;

      src += 4;
      dst += 1;
    }
  }
}

void RGBAF16To565(const uint16_t *sourceData, int srcStride,
                  uint16_t *destination, int dstStride, int width,
                  int height) {

  const auto maxColors = static_cast<float>((1 << 8) - 1);

  for (uint32_t y = 0; y < height; ++y) {
    auto src = reinterpret_cast<const uint16_t *>(reinterpret_cast<const uint8_t *>(sourceData)
        + y * srcStride);
    auto dst =
        reinterpret_cast<uint16_t *>( reinterpret_cast<uint8_t *>(destination) + y * dstStride);

    for (uint32_t x = 0; x < width; ++x) {
      uint8_t r = static_cast<uint8_t >(std::roundf(
          std::clamp(LoadHalf(src[0]), 0.0f, 1.0f) * maxColors));
      uint8_t g = static_cast<uint8_t >(std::roundf(
          std::clamp(LoadHalf(src[1]), 0.0f, 1.0f) * maxColors));
      uint8_t b = static_cast<uint8_t >(std::roundf(
          std::clamp(LoadHalf(src[2]), 0.0f, 1.0f) * maxColors));

      r = clamp(r, (uint8_t) 0, (uint8_t) maxColors);
      g = clamp(g, (uint8_t) 0, (uint8_t) maxColors);
      b = clamp(b, (uint8_t) 0, (uint8_t) maxColors);

      uint16_t red565 = (r >> 3) << 11;
      uint16_t green565 = (g >> 2) << 5;
      uint16_t blue565 = b >> 3;

      auto result = static_cast<uint16_t>(red565 | green565 | blue565);
      dst[0] = result;

      src += 4;
      dst += 1;
    }

  }
}
}