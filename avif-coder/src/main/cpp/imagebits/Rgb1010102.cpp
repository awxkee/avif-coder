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

#include "Rgb1010102.h"
#include <vector>
#include <thread>
#include <algorithm>
#include "half.hpp"
#include "concurrency.hpp"

using namespace std;

namespace coder {

template<typename V>
void RGBA1010102ToUnsigned(const uint8_t *__restrict__ src, const uint32_t srcStride,
                           V *__restrict__ dst, const uint32_t dstStride,
                           const uint32_t width, const uint32_t height, const uint32_t bitDepth) {
  auto mDstPointer = reinterpret_cast<uint8_t *>(dst);
  auto mSrcPointer = reinterpret_cast<const uint8_t *>(src);

  const auto maxColors = static_cast<float>((1 << bitDepth) - 1);
  const float valueScale = maxColors / 1023.f;
  const float alphaValueScale = maxColors / 3.f;

  const uint32_t mask = (1u << 10u) - 1u;

  for (uint32_t y = 0; y < height; ++y) {

    auto dstPointer = reinterpret_cast<V *>(mDstPointer);
    auto srcPointer = reinterpret_cast<const uint8_t *>(mSrcPointer);

    for (uint32_t x = 0; x < width; ++x) {
      uint32_t rgba1010102 = reinterpret_cast<const uint32_t *>(srcPointer)[0];

      auto r = static_cast<float>((rgba1010102) & mask);
      auto g = static_cast<float>((rgba1010102 >> 10) & mask);
      auto b = static_cast<float>((rgba1010102 >> 20) & mask);
      auto a1 = static_cast<float>((rgba1010102 >> 30));

      V ru = std::clamp(static_cast<V>(std::roundf(r * valueScale)), static_cast<V>(0),
                        static_cast<V>(maxColors));
      V gu = std::clamp(static_cast<V>(std::roundf(g * valueScale)), static_cast<V>(0),
                        static_cast<V>(maxColors));
      V bu = std::clamp(static_cast<V>(std::roundf(b * valueScale)), static_cast<V>(0),
                        static_cast<V>(maxColors));
      V au = std::clamp(static_cast<V>(std::roundf(a1 * alphaValueScale)),
                        static_cast<V>(0), static_cast<V>(maxColors));

      if (std::is_same<V, uint8_t>::value) {
        ru = (static_cast<uint16_t>(ru) * static_cast<uint16_t>(au)) / static_cast<uint16_t >(255);
        gu = (static_cast<uint16_t>(gu) * static_cast<uint16_t>(au)) / static_cast<uint16_t >(255);
        bu = (static_cast<uint16_t>(bu) * static_cast<uint16_t>(au)) / static_cast<uint16_t >(255);
      }

      dstPointer[0] = ru;
      dstPointer[1] = gu;
      dstPointer[2] = bu;
      dstPointer[3] = au;

      srcPointer += 4;
      dstPointer += 4;
    }

    mSrcPointer += srcStride;
    mDstPointer += dstStride;
  }
}

template void RGBA1010102ToUnsigned(const uint8_t *__restrict__ src,
                                    const uint32_t srcStride,
                                    uint8_t *__restrict__ dst,
                                    const uint32_t dstStride,
                                    const uint32_t width,
                                    const uint32_t height,
                                    const uint32_t bitDepth);

template void RGBA1010102ToUnsigned(const uint8_t *__restrict__ src,
                                    const uint32_t srcStride,
                                    uint16_t *__restrict__ dst,
                                    const uint32_t dstStride,
                                    const uint32_t width,
                                    const uint32_t height,
                                    const uint32_t bitDepth);

void
F16ToRGBA1010102(const uint16_t *source,
                 uint32_t srcStride,
                 uint8_t *destination,
                 uint32_t dstStride,
                 uint32_t width,
                 uint32_t height) {
  const auto range10 = static_cast<float>((1 << 10) - 1);
  for (uint32_t y = 0; y < height; ++y) {
    auto data = reinterpret_cast<const uint16_t *>(reinterpret_cast<const uint8_t *>(source)
        + y * srcStride);
    auto dst32 =
        reinterpret_cast<uint32_t *>(reinterpret_cast<uint8_t *>(destination) + y * dstStride);

    for (uint32_t x = 0; x < width; ++x) {
      auto R16 = (float) LoadHalf(data[0]);
      auto G16 = (float) LoadHalf(data[1]);
      auto B16 = (float) LoadHalf(data[2]);
      auto A16 = (float) LoadHalf(data[3]);
      auto R10 = static_cast<uint32_t>(std::clamp(std::roundf(R16 * range10), 0.0f,
                                                  (float) range10));
      auto G10 = static_cast<uint32_t>(std::clamp(std::roundf(G16 * range10), 0.0f,
                                                  (float) range10));
      auto B10 = static_cast<uint32_t>(std::clamp(std::roundf(B16 * range10), 0.0f,
                                                  (float) range10));
      auto A10 = static_cast<uint32_t>(std::clamp(std::roundf(A16 * 3.f), 0.0f, 3.0f));

      dst32[0] = (A10 << 30) | (B10 << 20) | (G10 << 10) | R10;

      data += 4;
      dst32 += 1;
    }
  }
}

void
Rgba8ToRGBA1010102(const uint8_t *source,
                   uint32_t srcStride,
                   uint8_t *destination,
                   uint32_t dstStride,
                   uint32_t width,
                   uint32_t height,
                   const bool attenuateAlpha) {

  for (uint32_t y = 0; y < height; ++y) {
    auto data = reinterpret_cast<const uint8_t *>(reinterpret_cast<const uint8_t *>(source)
        + y * srcStride);
    auto dst32 =
        reinterpret_cast<uint32_t *>(reinterpret_cast<uint8_t *>(destination) + y * dstStride);
    for (uint32_t x = 0; x < width; ++x) {
      uint8_t alpha = data[3];
      uint8_t r = data[0];
      uint8_t g = data[1];
      uint8_t b = data[2];

      if (attenuateAlpha) {
        r = (static_cast<uint16_t>(r) * static_cast<uint16_t>(alpha)) / static_cast<uint16_t >(255);
        g = (static_cast<uint16_t>(g) * static_cast<uint16_t>(alpha)) / static_cast<uint16_t >(255);
        b = (static_cast<uint16_t>(b) * static_cast<uint16_t>(alpha)) / static_cast<uint16_t >(255);
      }

      auto A16 = (uint32_t) alpha >> 6;
      auto R16 = (uint32_t) r << 2;
      auto G16 = (uint32_t) g << 2;
      auto B16 = (uint32_t) b << 2;

      dst32[0] = (A16 << 30) | (B16 << 20) | (G16 << 10) | R16;
      data += 4;
      dst32 += 1;
    }
  }
}

void
Rgba16ToRGBA1010102(const uint16_t *source,
                   uint32_t srcStride,
                   uint8_t *destination,
                   uint32_t dstStride,
                   uint32_t width,
                   uint32_t height,
                   uint32_t bitDepth) {

  int diff = static_cast<int>(bitDepth) - 10;
  int alphaDiff = static_cast<int>(bitDepth) - 2;

  for (uint32_t y = 0; y < height; ++y) {
    auto data = reinterpret_cast<const uint16_t *>(reinterpret_cast<const uint8_t *>(source)
        + y * srcStride);
    auto dst32 =
        reinterpret_cast<uint32_t *>(reinterpret_cast<uint8_t *>(destination) + y * dstStride);
    for (uint32_t x = 0; x < width; ++x) {
      uint16_t alpha = data[3];
      uint16_t r = data[0];
      uint16_t g = data[1];
      uint16_t b = data[2];

      auto A16 = (uint32_t) alpha >> alphaDiff;
      auto R16 = (uint32_t) r >> diff;
      auto G16 = (uint32_t) g >> diff;
      auto B16 = (uint32_t) b >> diff;

      dst32[0] =  (A16 & 0x3) << 30 | (B16 & 0x3ff) << 20 | (G16 & 0x3ff) << 10 | (R16 & 0x3ff);
      data += 4;
      dst32 += 1;
    }
  }
}

}