//
// Created by Radzivon Bartoshyk on 12/09/2023.
//

#include "CopyUnalignedRGBA.h"
#include "ThreadPool.hpp"
#include <cstdint>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "CopyUnalignedRGBA.cpp"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace coder {
    namespace HWY_NAMESPACE {

        using hwy::HWY_NAMESPACE::ScalableTag;
        using hwy::HWY_NAMESPACE::Store;
        using hwy::HWY_NAMESPACE::Load;
        using hwy::HWY_NAMESPACE::Vec;
        using hwy::HWY_NAMESPACE::TFromD;

        template<class D, typename T = TFromD<D>>
        void
        CopyUnalignedRGBARow(const D d, const T *HWY_RESTRICT src, T *HWY_RESTRICT dst, int width) {
            int x = 0;
            using VU = Vec<decltype(d)>;
            int pixels = d.MaxLanes() / 4;
            for (x = 0; x + pixels < width; x += pixels) {
                VU pixel = Load(d, src);
                Store(pixel, d, dst);

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
            ThreadPool pool;
            std::vector<std::future<void>> results;

            for (int y = 0; y < height; y++) {
                if (pixelSize == 1) {
                    const ScalableTag<uint8_t> du8;
                    auto fn = CopyUnalignedRGBARow<decltype(du8)>;
                    auto r = pool.enqueue(fn,
                                          du8,
                                          reinterpret_cast<const uint8_t *>(src + (y * srcStride)),
                                          reinterpret_cast<uint8_t *>(dst + (y * dstStride)),
                                          width);
                    results.push_back(std::move(r));
                } else if (pixelSize == 2) {
                    const ScalableTag<uint16_t> du16;
                    auto fn = CopyUnalignedRGBARow<decltype(du16)>;
                    auto r = pool.enqueue(fn,
                                          du16,
                                          reinterpret_cast<const uint16_t *>(src + (y * srcStride)),
                                          reinterpret_cast<uint16_t *>(dst + (y * dstStride)),
                                          width);
                    results.push_back(std::move(r));
                } else if (pixelSize == 4) {
                    const ScalableTag<float> df32;
                    auto fn = CopyUnalignedRGBARow<decltype(df32)>;
                    auto r = pool.enqueue(fn,
                                          df32,
                                          reinterpret_cast<const float *>(src + (y * srcStride)),
                                          reinterpret_cast<float *>(dst + (y * dstStride)),
                                          width);
                    results.push_back(std::move(r));
                }
            }

            for (auto &result: results) {
                result.wait();
            }
        }

    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(CopyUnalignedRGBA);

    HWY_DLLEXPORT void
    CopyUnalignedRGBA(const uint8_t *HWY_RESTRICT src, int srcStride, uint8_t *HWY_RESTRICT dst,
                      int dstStride, int width,
                      int height,
                      int pixelSize) {
        HWY_DYNAMIC_DISPATCH(CopyUnalignedRGBA)(src, srcStride, dst, dstStride, width, height,
                                                pixelSize);
    }
}
#endif