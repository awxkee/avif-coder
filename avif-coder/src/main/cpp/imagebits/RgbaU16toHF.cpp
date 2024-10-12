/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 22/11/2023
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

#include "RgbaU16toHF.h"
#include "concurrency.hpp"

using namespace std;

#include "half.hpp"
#include <thread>
#if HAVE_NEON
#include "arm_neon.h"
#endif

namespace coder {
void RgbaU16ToF(const uint16_t *src, const uint32_t srcStride,
                uint16_t *dst, const uint32_t dstStride, const uint32_t width,
                const uint32_t height, const uint32_t bitDepth) {

  const float scale = 1.f / static_cast<float>((1 << bitDepth) - 1);

  for (uint32_t y = 0; y < height; ++y) {
    auto vSrc =
        reinterpret_cast<const uint16_t *>(reinterpret_cast<const uint8_t *>(src) + srcStride * y);
    auto vDst =
        reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t *>(dst) + dstStride * y);

    uint32_t x = 0;

#if HAVE_NEON

    float32x4_t vScale = vdupq_n_f32(scale);

    for (; x + 8 < width; x += 8) {
      uint16x8x4_t pixelSet = vld4q_u16(vSrc);
      float32x4_t rHi = vmulq_f32(vcvtq_f32_u32(vmovl_high_u16(pixelSet.val[0])), vScale);
      float32x4_t rLo = vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(pixelSet.val[0]))), vScale);

      float32x4_t gHi = vmulq_f32(vcvtq_f32_u32(vmovl_high_u16(pixelSet.val[1])), vScale);
      float32x4_t gLo = vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(pixelSet.val[1]))), vScale);

      float32x4_t bHi = vmulq_f32(vcvtq_f32_u32(vmovl_high_u16(pixelSet.val[2])), vScale);
      float32x4_t bLo = vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(pixelSet.val[2]))), vScale);

      float32x4_t aHi = vmulq_f32(vcvtq_f32_u32(vmovl_high_u16(pixelSet.val[3])), vScale);
      float32x4_t aLo = vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(pixelSet.val[3]))), vScale);

      uint16x8_t rRow = vcombine_u16(
          vreinterpret_u16_f16(vcvt_f16_f32(rLo)),
          vreinterpret_u16_f16(vcvt_f16_f32(rHi))
      );

      uint16x8_t gRow = vcombine_u16(
          vreinterpret_u16_f16(vcvt_f16_f32(gLo)),
          vreinterpret_u16_f16(vcvt_f16_f32(gHi))
      );

      uint16x8_t bRow = vcombine_u16(
          vreinterpret_u16_f16(vcvt_f16_f32(bLo)),
          vreinterpret_u16_f16(vcvt_f16_f32(bHi))
      );

      uint16x8_t aRow = vcombine_u16(
          vreinterpret_u16_f16(vcvt_f16_f32(aLo)),
          vreinterpret_u16_f16(vcvt_f16_f32(aHi))
      );

      pixelSet.val[0] = rRow;
      pixelSet.val[1] = gRow;
      pixelSet.val[2] = bRow;
      pixelSet.val[3] = aRow;
      vst4q_u16(vDst, pixelSet);

      vSrc += 8 * 4;
      vDst += 8 * 4;
    }

#endif

    for (; x < width; ++x) {
#if HAVE_NEON
      auto vF16Dst = reinterpret_cast<float16_t *>(vDst);
      vF16Dst[0] = static_cast<float16_t >(static_cast<float>(vSrc[0]) * scale);
      vF16Dst[1] = static_cast<float16_t >(static_cast<float>(vSrc[1]) * scale);
      vF16Dst[2] = static_cast<float16_t >(static_cast<float>(vSrc[2]) * scale);
      vF16Dst[3] = static_cast<float16_t >(static_cast<float>(vSrc[3]) * scale);
#else
      vDst[0] = half_float::half(static_cast<float>(vSrc[0]) * scale).data_;
      vDst[1] = half_float::half(static_cast<float>(vSrc[1]) * scale).data_;
      vDst[0] = half_float::half(static_cast<float>(vSrc[2]) * scale).data_;
      vDst[3] = half_float::half(static_cast<float>(vSrc[3]) * scale).data_;
#endif

      vSrc += 4;
      vDst += 4;
    }
  }
}
}