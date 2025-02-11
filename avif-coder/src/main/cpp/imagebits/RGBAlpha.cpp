/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 06/11/2023
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

#include "RGBAlpha.h"
#include "concurrency.hpp"

using namespace std;

namespace coder {

void UnassociateRgba8(const uint8_t *src, uint32_t srcStride,
                      uint8_t *dst, uint32_t dstStride, uint32_t width,
                      uint32_t height) {
  for (uint32_t y = 0; y < height; ++y) {
    auto mSrc = reinterpret_cast<const uint8_t *>(src) + y * srcStride;
    auto mDst = reinterpret_cast<uint8_t *>(dst) + y * dstStride;

    uint32_t x = 0;

    for (; x < width; ++x) {
      uint8_t alpha = mSrc[3];

      if (alpha != 0) {
        mDst[0] = (static_cast<uint16_t >(mSrc[0]) * static_cast<uint16_t >(255))
            / static_cast<uint16_t >(alpha);
        mDst[1] = (static_cast<uint16_t >(mSrc[1]) * static_cast<uint16_t >(255))
            / static_cast<uint16_t >(alpha);
        mDst[2] = (static_cast<uint16_t >(mSrc[2]) * static_cast<uint16_t >(255))
            / static_cast<uint16_t >(alpha);
      } else {
        mDst[0] = 0;
        mDst[1] = 0;
        mDst[2] = 0;
      }
      mDst[3] = alpha;
      mSrc += 4;
      mDst += 4;
    }
  }
}

void AssociateAlphaRgba8(const uint8_t *src, uint32_t srcStride,
                         uint8_t *dst, uint32_t dstStride, uint32_t width,
                         uint32_t height) {
  for (uint32_t y = 0; y < height; ++y) {

    auto mSrc = reinterpret_cast<const uint8_t *>(src) + y * srcStride;
    auto mDst = reinterpret_cast<uint8_t *>(dst) + y * dstStride;

    uint32_t x = 0;

    for (; x < width; ++x) {
      uint8_t alpha = mSrc[3];
      mDst[0] = (static_cast<uint16_t>(mSrc[0]) * static_cast<uint16_t>(alpha))
          / static_cast<uint16_t >(255);
      mDst[1] = (static_cast<uint16_t>(mSrc[1]) * static_cast<uint16_t>(alpha))
          / static_cast<uint16_t >(255);
      mDst[2] = (static_cast<uint16_t>(mSrc[2]) * static_cast<uint16_t>(alpha))
          / static_cast<uint16_t >(255);
      mDst[3] = alpha;
      mSrc += 4;
      mDst += 4;
    }
  }
}

void AssociateAlphaRgba16(const uint16_t *src, uint32_t srcStride,
                          uint16_t *dst, uint32_t dstStride, uint32_t width,
                          uint32_t height, uint32_t bitDepth) {
  uint32_t maxColors = (1 << bitDepth) - 1;
  for (uint32_t y = 0; y < height; ++y) {

    auto mSrc =
        reinterpret_cast<const uint16_t *>( reinterpret_cast<const uint8_t *>(src) + y * srcStride);
    auto mDst = reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t *>(dst) + y * dstStride);

    uint32_t x = 0;

    for (; x < width; ++x) {
      uint16_t alpha = mSrc[3];
      mDst[0] = (static_cast<uint32_t>(mSrc[0]) * static_cast<uint32_t>(alpha))
          / static_cast<uint32_t >(maxColors);
      mDst[1] = (static_cast<uint32_t>(mSrc[1]) * static_cast<uint32_t>(alpha))
          / static_cast<uint32_t >(maxColors);
      mDst[2] = (static_cast<uint32_t>(mSrc[2]) * static_cast<uint32_t>(alpha))
          / static_cast<uint32_t >(maxColors);

      mDst[3] = alpha;
      mSrc += 4;
      mDst += 4;
    }
  }
}

}