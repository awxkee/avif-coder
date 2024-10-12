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

#include "Rgba8ToF16.h"
#include <thread>
#include <vector>
#include "half.hpp"
#include "concurrency.hpp"

#if HAVE_NEON
#include "arm_neon.h"
#endif

using namespace std;
using namespace half_float;

namespace coder {
void Rgba8ToF16(const uint8_t *sourceData, uint32_t srcStride,
                uint16_t *dst, uint32_t dstStride, uint32_t width,
                uint32_t height, const bool attenuateAlpha) {
  const float scale = 1.0f / float((1 << 8) - 1);

  for (uint32_t y = 0; y < height; ++y) {

    auto vSrc = reinterpret_cast<const uint8_t *>(reinterpret_cast<const uint8_t *>(sourceData)
        + y * srcStride);
    auto vDst =
        reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t *>(dst) + y * dstStride);

    for (uint32_t x = 0; x < width; ++x) {
      uint8_t alpha = vSrc[3];
      uint8_t r = vSrc[0];
      uint8_t g = vSrc[1];
      uint8_t b = vSrc[2];

      if (attenuateAlpha) {
        r = (static_cast<uint16_t>(r) * static_cast<uint16_t>(alpha)) / static_cast<uint16_t >(255);
        g = (static_cast<uint16_t>(g) * static_cast<uint16_t>(alpha)) / static_cast<uint16_t >(255);
        b = (static_cast<uint16_t>(b) * static_cast<uint16_t>(alpha)) / static_cast<uint16_t >(255);
      }

#if HAVE_NEON
      auto vF16Dst = reinterpret_cast<float16_t *>(vDst);
      auto tmpR = static_cast<float16_t>(static_cast<float>(r) * scale);
      auto tmpG = static_cast<float16_t>(static_cast<float>(g) * scale);
      auto tmpB = static_cast<float16_t>(static_cast<float>(b) * scale);
      auto tmpA = static_cast<float16_t>(static_cast<float>(alpha) * scale);

      float16_t clr[4] = {tmpR, tmpG, tmpB, tmpA};

      vF16Dst[0] = clr[0];
      vF16Dst[1] = clr[1];
      vF16Dst[2] = clr[2];
      vF16Dst[3] = clr[3];
#else
      uint16_t tmpR = (uint16_t) half(static_cast<float>(r) * scale).data_;
      uint16_t tmpG = (uint16_t) half(static_cast<float>(g) * scale).data_;
      uint16_t tmpB = (uint16_t) half(static_cast<float>(b) * scale).data_;
      uint16_t tmpA = (uint16_t) half(static_cast<float>(alpha) * scale).data_;

      uint16_t clr[4] = {tmpR, tmpG, tmpB, tmpA};

      vDst[0] = clr[0];
      vDst[1] = clr[1];
      vDst[2] = clr[2];
      vDst[3] = clr[3];
#endif

      vSrc += 4;
      vDst += 4;
    }
  }
}
}