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

#include "RgbaF16bitNBitU8.h"
#include "half.hpp"
#include <algorithm>
#include <thread>
#include <vector>
#include "concurrency.hpp"
#if HAVE_NEON
#include "arm_neon.h"
#endif

using namespace std;

namespace coder {
void RGBAF16BitToNBitU8(const uint16_t *sourceData, uint32_t srcStride,
                        uint8_t *dst, uint32_t dstStride, uint32_t width,
                        uint32_t height, uint32_t bitDepth, const bool attenuateAlpha) {
  const auto maxColors = static_cast<float>((1 << bitDepth) - 1);

  for (uint32_t y = 0; y < height; ++y) {
#if HAVE_NEON
    auto vSrc = reinterpret_cast<const float16_t *>(reinterpret_cast<const uint8_t *>(sourceData)
        + y * srcStride);
    auto vDst =
        reinterpret_cast<uint8_t *>(reinterpret_cast<uint8_t *>(dst) + y * dstStride);
    for (uint32_t x = 0; x < width; ++x) {
      auto tmpR = (uint8_t) std::clamp(std::roundf(static_cast<float>(vSrc[0]) * maxColors),
                                       0.0f,
                                       maxColors);
      auto tmpG = (uint8_t) std::clamp(std::roundf(static_cast<float>(vSrc[1]) * maxColors),
                                       0.0f,
                                       maxColors);
      auto tmpB = (uint8_t) std::clamp(std::roundf(static_cast<float>(vSrc[2]) * maxColors),
                                       0.0f,
                                       maxColors);
      auto tmpA = (uint8_t) std::clamp(std::roundf(static_cast<float>(vSrc[3]) * maxColors),
                                       0.0f,
                                       maxColors);

      if (attenuateAlpha) {
        tmpR = (static_cast<uint16_t>(tmpR) * static_cast<uint16_t>(tmpA))
            / static_cast<uint16_t >(255);
        tmpG = (static_cast<uint16_t>(tmpG) * static_cast<uint16_t>(tmpA))
            / static_cast<uint16_t >(255);
        tmpB = (static_cast<uint16_t>(tmpB) * static_cast<uint16_t>(tmpA))
            / static_cast<uint16_t >(255);
      }

      vDst[0] = tmpR;
      vDst[1] = tmpG;
      vDst[2] = tmpB;
      vDst[3] = tmpA;

      vSrc += 4;
      vDst += 4;
    }
#else
    auto vSrc = reinterpret_cast<const uint16_t *>(reinterpret_cast<const uint8_t *>(sourceData)
        + y * srcStride);
    auto vDst =
        reinterpret_cast<uint8_t *>(reinterpret_cast<uint8_t *>(dst) + y * dstStride);
    for (uint32_t x = 0; x < width; ++x) {
      auto tmpR = (uint8_t) std::clamp(std::roundf(LoadHalf(vSrc[0]) * maxColors), 0.0f, maxColors);
      auto tmpG = (uint8_t) std::clamp(std::roundf(LoadHalf(vSrc[1]) * maxColors), 0.0f, maxColors);
      auto tmpB = (uint8_t) std::clamp(std::roundf(LoadHalf(vSrc[2]) * maxColors), 0.0f, maxColors);
      auto tmpA = (uint8_t) std::clamp(std::roundf(LoadHalf(vSrc[3]) * maxColors), 0.0f, maxColors);

      if (attenuateAlpha) {
        tmpR = (static_cast<uint16_t>(tmpR) * static_cast<uint16_t>(tmpA))
            / static_cast<uint16_t >(255);
        tmpG = (static_cast<uint16_t>(tmpG) * static_cast<uint16_t>(tmpA))
            / static_cast<uint16_t >(255);
        tmpB = (static_cast<uint16_t>(tmpB) * static_cast<uint16_t>(tmpA))
            / static_cast<uint16_t >(255);
      }

      vDst[0] = tmpR;
      vDst[1] = tmpG;
      vDst[2] = tmpB;
      vDst[3] = tmpA;

      vSrc += 4;
      vDst += 4;
    }
#endif
  }
}
}