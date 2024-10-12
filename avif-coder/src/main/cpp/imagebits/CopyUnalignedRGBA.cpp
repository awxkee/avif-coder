/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 12/09/2023
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

#include "CopyUnalignedRGBA.h"
#include <cstdint>
#include <thread>
#include <vector>
#include "concurrency.hpp"

using namespace std;

namespace coder {

template<typename T>
void
CopyUnaligned(const T *src, uint32_t srcStride, T *dst,
              uint32_t dstStride, uint32_t width,
              uint32_t height) {
  for (uint32_t y = 0; y < height; ++y) {
    auto vSrc = reinterpret_cast<const T *>(reinterpret_cast<const uint8_t *>(src) + y * srcStride);
    auto vDst = reinterpret_cast<T *>(reinterpret_cast<uint8_t *>(dst) + y * dstStride);

    std::copy(vSrc, vSrc + width, vDst);
  }

}

template void
CopyUnaligned(const uint8_t *src, uint32_t srcStride, uint8_t *dst,
              uint32_t dstStride, uint32_t width,
              uint32_t height);

template void
CopyUnaligned(const uint16_t *src, uint32_t srcStride, uint16_t *dst,
              uint32_t dstStride, uint32_t width,
              uint32_t height);

template void
CopyUnaligned(const uint32_t *src, uint32_t srcStride, uint32_t *dst,
              uint32_t dstStride, uint32_t width,
              uint32_t height);

}