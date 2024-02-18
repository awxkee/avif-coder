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

using namespace std;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "imagebits/CopyUnalignedRGBA.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

    using hwy::HWY_NAMESPACE::ScalableTag;
    using hwy::HWY_NAMESPACE::StoreU;
    using hwy::HWY_NAMESPACE::LoadU;
    using hwy::HWY_NAMESPACE::FixedTag;
    using hwy::HWY_NAMESPACE::Vec;
    using hwy::HWY_NAMESPACE::LoadInterleaved4;
    using hwy::HWY_NAMESPACE::StoreInterleaved4;

    template<class D, typename Buf, typename T = Vec<D>>
    void
    Copy1Row(const D d, const Buf *HWY_RESTRICT src, Buf *HWY_RESTRICT dst, int width) {
        int x = 0;
        using VU = Vec<decltype(d)>;
        int pixels = d.MaxLanes();
        for (; x + pixels < width; x += pixels) {
            VU a = LoadU(d, src);
            StoreU(a, d, reinterpret_cast<Buf *>(dst));

            src += pixels;
            dst += pixels;
        }

        for (; x < width; ++x) {
            auto p1 = src[0];
            dst[0] = p1;

            src += 1;
            dst += 1;
        }
    }

    void
    CopyUnalignedRGBA(const uint8_t *HWY_RESTRICT src, int srcStride, uint8_t *HWY_RESTRICT dst,
                      int dstStride,
                      int width,
                      int height,
                      int pixelSize) {
    #pragma omp parallel for num_threads(3) schedule(dynamic)
        for (int y = 0; y < height; ++y) {
            if (pixelSize == 1) {
                const ScalableTag<uint8_t> du8;
                Copy1Row<decltype(du8), uint8_t>(du8,
                                                 reinterpret_cast<const uint8_t *>(
                                                         reinterpret_cast<const uint8_t *>(src) +
                                                         (y * srcStride)),
                                                 reinterpret_cast<uint8_t *>(
                                                         reinterpret_cast<uint8_t *>(dst) +
                                                         (y * dstStride)),
                                                 width);
            } else if (pixelSize == 2) {
                const ScalableTag<uint16_t> du16;
                Copy1Row<decltype(du16), uint16_t>(du16,
                                                   reinterpret_cast<const uint16_t *>(
                                                           reinterpret_cast<const uint8_t *>(src) +
                                                           (y * srcStride)),
                                                   reinterpret_cast<uint16_t *>(
                                                           reinterpret_cast<uint8_t *>(dst) +
                                                           (y * dstStride)),
                                                   width);
            } else if (pixelSize == 4) {
                const ScalableTag<uint32_t> df32;
                Copy1Row<decltype(df32), uint32_t>(df32,
                                                   reinterpret_cast<const uint32_t *>(
                                                           reinterpret_cast<const uint8_t *>(src) +
                                                           (y * srcStride)),
                                                   reinterpret_cast<uint32_t *>(
                                                           reinterpret_cast<uint8_t *>(dst) +
                                                           (y * dstStride)), width);
            }
        }
    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(CopyUnalignedRGBA);

    HWY_DLLEXPORT void
    CopyUnaligned(const uint8_t *HWY_RESTRICT src, int srcStride, uint8_t *HWY_RESTRICT dst,
                  int dstStride, int width,
                  int height,
                  int pixelSize) {
        HWY_DYNAMIC_DISPATCH(CopyUnalignedRGBA)(src, srcStride, dst, dstStride, width, height,
                                                pixelSize);
    }

}
#endif