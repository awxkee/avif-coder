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

#include "RgbaF16bitToNBitU16.h"
#include "half.hpp"
#include <algorithm>
#include "concurrency.hpp"
#if HAVE_NEON
#include "arm_neon.h"
#endif

using namespace std;

namespace coder {

void
RGBAF16BitToNBitU16(const uint16_t *sourceData,
                    uint32_t srcStride,
                    uint16_t *dst,
                    uint32_t dstStride,
                    uint32_t width,
                    uint32_t height,
                    uint32_t bitDepth) {
  auto srcData = reinterpret_cast<const uint8_t *>(sourceData);
  auto data64Ptr = reinterpret_cast<uint8_t *>(dst);
  const float scale = 1.0f / float((1 << bitDepth) - 1);
  const float maxColors = (float) std::powf(2.0f, static_cast<float>(bitDepth)) - 1.f;

  for (uint32_t y = 0; y < height; ++y) {

#if HAVE_NEON
    auto srcPtr = reinterpret_cast<const float16_t *>(srcData);
    auto dstPtr = reinterpret_cast<uint16_t *>(data64Ptr);
    for (int x = 0; x < width; ++x) {
      auto alpha = static_cast<float16_t >(srcPtr[3]);
      auto tmpR =
          static_cast<uint16_t>(std::clamp(static_cast<float16_t >(srcPtr[0]) * maxColors, 0.0f,
                                           maxColors));
      auto tmpG =
          static_cast<uint16_t>(std::clamp(static_cast<float16_t >(srcPtr[1]) * maxColors, 0.0f,
                                           maxColors));
      auto tmpB =
          static_cast<uint16_t>(std::clamp(static_cast<float16_t >(srcPtr[2]) * maxColors, 0.0f,
                                           maxColors));
      auto tmpA = static_cast<uint16_t>(std::clamp((alpha / scale), 0.0f, maxColors));

      dstPtr[0] = tmpR;
      dstPtr[1] = tmpG;
      dstPtr[2] = tmpB;
      dstPtr[3] = tmpA;

      srcPtr += 4;
      dstPtr += 4;
    }
#else
    auto srcPtr = reinterpret_cast<const uint16_t *>(srcData);
    auto dstPtr = reinterpret_cast<uint16_t *>(data64Ptr);
    for (int x = 0; x < width; ++x) {
      auto alpha = LoadHalf(srcPtr[3]);
      auto tmpR = static_cast<uint16_t>(std::clamp(LoadHalf(srcPtr[0]) * maxColors, 0.0f,
                                                   maxColors));
      auto tmpG = static_cast<uint16_t>(std::clamp(LoadHalf(srcPtr[1]) * maxColors, 0.0f,
                                                   maxColors));
      auto tmpB = static_cast<uint16_t>(std::clamp(LoadHalf(srcPtr[2]) * maxColors, 0.0f,
                                                   maxColors));
      auto tmpA = static_cast<uint16_t>(std::clamp((alpha / scale), 0.0f, maxColors));

      dstPtr[0] = tmpR;
      dstPtr[1] = tmpG;
      dstPtr[2] = tmpB;
      dstPtr[3] = tmpA;

      srcPtr += 4;
      dstPtr += 4;
    }
#endif

    srcData += srcStride;
    data64Ptr += dstStride;
  }
}

}