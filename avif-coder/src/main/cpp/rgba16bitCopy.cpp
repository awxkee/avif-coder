//
// Created by Radzivon Bartoshyk on 05/09/2023.
//

#include "rgba16bitCopy.h"
#include <vector>
#include <cstdint>

#if HAVE_NEON

#include <arm_neon.h>

void copyRGBA16_NEON(uint16_t* source, int srcStride,
                     uint16_t *destination, int dstStride,
                     int width, int height) {
    auto src = reinterpret_cast<uint8_t *>(source);
    auto dst = reinterpret_cast<uint8_t *>(destination);

    for (int y = 0; y < height; ++y) {

        auto srcPtr = reinterpret_cast<uint16_t *>(src);
        auto dstPtr = reinterpret_cast<uint16_t *>(dst);
        int x;

        for (x = 0; x < width; x += 2) {
            uint16x8_t neonSrc = vld1q_u16(srcPtr);
            vst1q_u16(dstPtr, neonSrc);
            srcPtr += 8;
            dstPtr += 8;
        }

        for (x = 0; x < width; ++x) {
            auto srcPtr64 = reinterpret_cast<uint64_t *>(srcPtr);
            auto dstPtr64 = reinterpret_cast<uint64_t *>(dstPtr);
            dstPtr64[0] = srcPtr64[0];
            srcPtr += 4;
            dstPtr += 4;
        }

        src += srcStride;
        dst += dstStride;
    }
}

#endif

void copyRGBA16_C(uint16_t* source, int srcStride,
                  uint16_t *destination, int dstStride,
                  int width, int height) {
    auto src = reinterpret_cast<uint8_t *>(source);
    auto dst = reinterpret_cast<uint8_t *>(destination);

    for (int y = 0; y < height; ++y) {

        auto srcPtr = reinterpret_cast<uint16_t *>(src);
        auto dstPtr = reinterpret_cast<uint16_t *>(dst);

        for (int x = 0; x < width; ++x) {
            auto srcPtr64 = reinterpret_cast<uint64_t *>(srcPtr);
            auto dstPtr64 = reinterpret_cast<uint64_t *>(dstPtr);
            dstPtr64[0] = srcPtr64[0];
            srcPtr += 4;
            dstPtr += 4;
        }

        src += srcStride;
        dst += dstStride;
    }
}

void copyRGBA16(uint16_t* source, int srcStride,
                uint16_t *destination, int dstStride,
                int width, int height) {
#if HAVE_NEON
    copyRGBA16_NEON(source, srcStride, destination, dstStride, width, height);
#else
    copyRGBA16_C(source, srcStride, destination, dstStride, width, height);
#endif
}