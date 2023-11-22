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
    using hwy::HWY_NAMESPACE::Store;
    using hwy::HWY_NAMESPACE::Load;
    using hwy::HWY_NAMESPACE::Vec;
    using hwy::HWY_NAMESPACE::LoadInterleaved4;
    using hwy::HWY_NAMESPACE::StoreInterleaved4;

    template<class D, typename Buf, typename T = Vec<D>>
    void
    CopyUnalignedRGBARow(const D d, const Buf *HWY_RESTRICT src, Buf *HWY_RESTRICT dst, int width) {
        int x = 0;
        using VU = Vec<decltype(d)>;
        int pixels = d.MaxLanes();
        for (; x + pixels < width; x += pixels) {
            VU a, r, g, b;
            LoadInterleaved4(d, src, a, r, g, b);
            StoreInterleaved4(a, r, g, b, d, dst);

            src += pixels * 4;
            dst += pixels * 4;
        }

        for (; x < width; ++x) {
            auto p1 = src[0];
            auto p2 = src[1];
            auto p3 = src[2];
            auto p4 = src[3];

            dst[0] = p1;
            dst[1] = p2;
            dst[2] = p3;
            dst[3] = p4;

            src += 4;
            dst += 4;
        }
    }

    void
    CopyUnalignedRGBA(const uint8_t *HWY_RESTRICT src, int srcStride, uint8_t *HWY_RESTRICT dst,
                      int dstStride, int width,
                      int height,
                      int pixelSize) {
        int threadCount = clamp(min(static_cast<int>(std::thread::hardware_concurrency()),
                                    width * height / (256 * 256)), 1, 12);
        vector<thread> workers;

        int segmentHeight = height / threadCount;

        for (int i = 0; i < threadCount; i++) {
            int start = i * segmentHeight;
            int end = (i + 1) * segmentHeight;
            if (i == threadCount - 1) {
                end = height;
            }
            workers.emplace_back([start, end, src, dstStride, dst, srcStride, pixelSize, width]() {
                for (int y = start; y < end; ++y) {
                    if (pixelSize == 1) {
                        const ScalableTag<uint8_t> du8;
                        auto fn = CopyUnalignedRGBARow<decltype(du8), uint8_t>;
                        fn(du8,
                           reinterpret_cast<const uint8_t *>(
                                   reinterpret_cast<const uint8_t *>(src) + (y * srcStride)),
                           reinterpret_cast<uint8_t *>(reinterpret_cast<uint8_t *>(dst) +
                                                       (y * dstStride)),
                           width);
                    } else if (pixelSize == 2) {
                        const ScalableTag<uint16_t> du16;
                        auto fn = CopyUnalignedRGBARow<decltype(du16), uint16_t>;
                        fn(du16,
                           reinterpret_cast<const uint16_t *>(
                                   reinterpret_cast<const uint8_t *>(src) +
                                   (y * srcStride)),
                           reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t *>(dst) +
                                                        (y * dstStride)),
                           width);
                    } else if (pixelSize == 4) {
                        const ScalableTag<uint32_t> df32;
                        auto fn = CopyUnalignedRGBARow<decltype(df32), uint32_t>;
                        fn(df32,
                           reinterpret_cast<const uint32_t *>(
                                   reinterpret_cast<const uint8_t *>(src) +
                                   (y * srcStride)),
                           reinterpret_cast<uint32_t *>(reinterpret_cast<uint8_t *>(dst) +
                                                        (y * dstStride)),
                           width);
                    }
                }
            });
        }

        for (std::thread &thread: workers) {
            thread.join();
        }
    }

    void CopyUnalignedRGB565Row(const uint16_t *HWY_RESTRICT src, uint16_t *HWY_RESTRICT dst,
                                int width) {
        int x = 0;
        const ScalableTag<uint16_t> du;
        using VU = Vec<decltype(du)>;
        int pixels = du.MaxLanes();
        for (; x + pixels < width; x += pixels) {
            VU pixel = Load(du, src);
            Store(pixel, du, dst);

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
    CopyUnalignedRGB565(const uint8_t *HWY_RESTRICT src, int srcStride, uint8_t *HWY_RESTRICT dst,
                        int dstStride, int width,
                        int height) {
        int threadCount = clamp(min(static_cast<int>(std::thread::hardware_concurrency()),
                                    height * width / (356 * 356)), 1, 12);
        std::vector<std::thread> workers;
        int segmentHeight = height / threadCount;

        for (int i = 0; i < threadCount; i++) {
            int start = i * segmentHeight;
            int end = (i + 1) * segmentHeight;
            if (i == threadCount - 1) {
                end = height;
            }
            workers.emplace_back([start, end, src, dstStride, dst, srcStride, width]() {
                for (int y = start; y < end; ++y) {
                    CopyUnalignedRGB565Row(
                            reinterpret_cast<const uint16_t *>(src + (y * srcStride)),
                            reinterpret_cast<uint16_t *>(dst + (y * dstStride)),
                            width);
                }
            });
        }

        for (std::thread &thread: workers) {
            thread.join();
        }
    }

}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(CopyUnalignedRGBA);
    HWY_EXPORT(CopyUnalignedRGB565);

    HWY_DLLEXPORT void
    CopyUnalignedRGBA(const uint8_t *HWY_RESTRICT src, int srcStride, uint8_t *HWY_RESTRICT dst,
                      int dstStride, int width,
                      int height,
                      int pixelSize) {
        HWY_DYNAMIC_DISPATCH(CopyUnalignedRGBA)(src, srcStride, dst, dstStride, width, height,
                                                pixelSize);
    }

    HWY_DLLEXPORT void
    CopyUnalignedRGB565(const uint8_t *HWY_RESTRICT src, int srcStride, uint8_t *HWY_RESTRICT dst,
                        int dstStride, int width,
                        int height) {
        HWY_DYNAMIC_DISPATCH(CopyUnalignedRGB565)(src, srcStride, dst, dstStride, width, height);
    }
}
#endif