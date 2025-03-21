/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 02/09/2023
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

#include "colorspace/colorspace.h"
#include <cmath>
#include <vector>
#include <thread>
#include <android/log.h>
#include "concurrency.hpp"
#include "avifweaver.h"

void
convertUseICC(aligned_uint8_vector &vector, uint32_t stride, uint32_t width, uint32_t height,
              const unsigned char *colorSpace, size_t colorSpaceSize,
              bool image16Bits, uint16_t bitDepth) {
  aligned_uint8_vector target(vector.size());
  if (image16Bits) {
    apply_icc_rgba16(reinterpret_cast<uint16_t *>(vector.data()),
                     stride,
                     reinterpret_cast<uint16_t *>(target.data()),
                     stride,
                     bitDepth,
                     width,
                     height,
                     colorSpace,
                     colorSpaceSize);
  } else {
    apply_icc_rgba8(vector.data(),
                    stride,
                    target.data(),
                    stride,
                    width,
                    height,
                    colorSpace,
                    colorSpaceSize);
  }
  vector = std::move(target);
}
