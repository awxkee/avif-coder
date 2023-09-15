//
// Created by Radzivon Bartoshyk on 05/09/2023.
//

#include "RgbaF16bitNBitU8.h"
#include "HalfFloats.h"
#include <algorithm>

#if HAVE_NEON

#include <arm_neon.h>

void RGBAF16BitToNU8NEON(const uint16_t *sourceData, int srcStride,
                         uint8_t *dst, int dstStride,
                         int width, int height, int bitDepth) {
    auto srcData = reinterpret_cast<const uint8_t *>(sourceData);
    auto data64Ptr = reinterpret_cast<uint8_t *>(dst);
    const float scale = 1.0f / float((1 << bitDepth) - 1);
    auto vectorScale = vdupq_n_f32(scale);
    float maxColors = (float) pow(2.0, (double) bitDepth) - 1;
    auto maxValue = vdupq_n_f32((float) maxColors);
    auto minValue = vdupq_n_f32(0);

    for (int y = 0; y < height; ++y) {
        auto srcPtr = reinterpret_cast<const uint16_t *>(srcData);
        auto dstPtr = reinterpret_cast<uint8_t *>(data64Ptr);
        int x;
        for (x = 0; x + 2 < width; x += 2) {
            float16x8_t neonSrc = vld1q_f16(reinterpret_cast<const __fp16 *>(srcPtr));

            float32x4_t neonFloat32_1 = vminq_f32(
                    vmaxq_f32(vdivq_f32(vcvt_f32_f16(vget_low_f16(neonSrc)), vectorScale),
                              minValue), maxValue);
            float32x4_t neonFloat32_2 = vminq_f32(
                    vmaxq_f32(vdivq_f32(vcvt_f32_f16(vget_high_f16(neonSrc)), vectorScale),
                              minValue), maxValue);

            int32x4_t neonSigned1 = vcvtq_s32_f32(neonFloat32_1);
            int32x4_t neonSigned2 = vcvtq_s32_f32(neonFloat32_2);

            uint8x8_t neonUInt8 = vqmovun_s16(vcombine_s16(
                    vqmovn_s32(neonSigned1),
                    vqmovn_s32(neonSigned2)
            ));

            vst1_u8(dstPtr, neonUInt8);

            dstPtr += 8;
            srcPtr += 8;
        }

        for (; x < width; ++x) {
            auto alpha = half_to_float(srcPtr[3]);
            auto tmpR = (uint16_t) std::clamp(half_to_float(srcPtr[0]) / scale, 0.0f, maxColors);
            auto tmpG = (uint16_t) std::clamp(half_to_float(srcPtr[1]) / scale, 0.0f, maxColors);
            auto tmpB = (uint16_t) std::clamp(half_to_float(srcPtr[2]) / scale, 0.0f, maxColors);
            auto tmpA = (uint16_t) std::clamp(alpha / scale, 0.0f, maxColors);

            dstPtr[0] = tmpR;
            dstPtr[1] = tmpG;
            dstPtr[2] = tmpB;
            dstPtr[3] = tmpA;

            srcPtr += 4;
            dstPtr += 4;
        }

        srcData += srcStride;
        data64Ptr += dstStride;
    }
}

#endif

void RGBAF16BitToNU8C(const uint16_t *sourceData, int srcStride,
                      uint8_t *dst, int dstStride, int width,
                      int height, int bitDepth) {
    auto srcData = reinterpret_cast<const uint8_t *>(sourceData);
    auto data64Ptr = reinterpret_cast<uint8_t *>(dst);
    const float scale = 1.0f / float((1 << bitDepth) - 1);
    float maxColors = (float) pow(2.0, (double) bitDepth) - 1;

    for (int y = 0; y < height; ++y) {
        auto srcPtr = reinterpret_cast<const uint16_t *>(srcData);
        auto dstPtr = reinterpret_cast<uint8_t *>(data64Ptr);
        for (int x = 0; x < width; ++x) {
            auto alpha = half_to_float(srcPtr[3]);
            auto tmpR = (uint8_t) std::clamp(half_to_float(srcPtr[0]) / scale, 0.0f, maxColors);
            auto tmpG = (uint8_t) std::clamp(half_to_float(srcPtr[1]) / scale, 0.0f, maxColors);
            auto tmpB = (uint8_t) std::clamp(half_to_float(srcPtr[2]) / scale, 0.0f, maxColors);
            auto tmpA = (uint8_t) std::clamp(alpha / scale, 0.0f, maxColors);

            dstPtr[0] = tmpR;
            dstPtr[1] = tmpG;
            dstPtr[2] = tmpB;
            dstPtr[3] = tmpA;

            srcPtr += 4;
            dstPtr += 4;
        }

        srcData += srcStride;
        data64Ptr += dstStride;
    }
}

void RGBAF16BitToNBitU8(const uint16_t *sourceData, int srcStride,
                        uint8_t *dst, int dstStride, int width,
                        int height, int bitDepth) {
#if HAVE_NEON
    RGBAF16BitToNU8NEON(sourceData, srcStride, dst, dstStride, width, height, bitDepth);
#else
    RGBAF16BitToNU8C(sourceData, srcStride, dst, dstStride, width, height, bitDepth);
#endif
}