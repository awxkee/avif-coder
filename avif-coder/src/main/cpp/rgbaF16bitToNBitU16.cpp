//
// Created by Radzivon Bartoshyk on 05/09/2023.
//

#include "rgbaF16bitToNBitU16.h"
#include "halfFloats.h"

#if HAVE_NEON

#include <arm_neon.h>

void RGBAF16BitToNU16_NEON(const uint16_t *sourceData, int srcStride, uint16_t *dst, int dstStride,
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
        auto dstPtr = reinterpret_cast<uint16_t *>(data64Ptr);
        int x;
        for (x = 0; x < width; x += 2) {
            float16x8_t neonSrc = vld1q_f16(reinterpret_cast<const __fp16 *>(srcPtr));

            float32x4_t neonFloat32_1 = vminq_f32(
                    vmaxq_f32(vdivq_f32(vcvt_f32_f16(vget_low_f16(neonSrc)), vectorScale),
                              minValue), maxValue);
            float32x4_t neonFloat32_2 = vminq_f32(
                    vmaxq_f32(vdivq_f32(vcvt_f32_f16(vget_high_f16(neonSrc)), vectorScale),
                              minValue), maxValue);

            int32x4_t neonSigned1 = vcvtq_s32_f32(neonFloat32_1);
            int32x4_t neonSigned2 = vcvtq_s32_f32(neonFloat32_2);

            uint16x4_t neonUInt16_1 = vqmovn_u32(vreinterpretq_u32_s32(neonSigned1));
            uint16x4_t neonUInt16_2 = vqmovn_u32(vreinterpretq_u32_s32(neonSigned2));

            vst1_u16(dstPtr, neonUInt16_1);
            vst1_u16(dstPtr + 4, neonUInt16_2);

            srcPtr += 8;
            dstPtr += 8;
        }

        for (x = 0; x < width; ++x) {
            auto alpha = half_to_float(srcPtr[3]);
            auto tmpR = (uint16_t) std::min(std::max((half_to_float(srcPtr[0]) / scale), 0.0f),
                                            maxColors);
            auto tmpG = (uint16_t) std::min(std::max((half_to_float(srcPtr[1]) / scale), 0.0f),
                                            maxColors);
            auto tmpB = (uint16_t) std::min(std::max((half_to_float(srcPtr[2]) / scale), 0.0f),
                                            maxColors);
            auto tmpA = (uint16_t) std::min(std::max((alpha / scale), 0.0f), maxColors);

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

void RGBAF16BitToNU16_C(const uint16_t *sourceData, int srcStride,
                        uint16_t *dst, int dstStride, int width,
                        int height, int bitDepth) {
    auto srcData = reinterpret_cast<const uint8_t *>(sourceData);
    auto data64Ptr = reinterpret_cast<uint8_t *>(dst);
    const float scale = 1.0f / float((1 << bitDepth) - 1);
    float maxColors = (float)pow(2.0, (double) bitDepth) - 1;

    for (int y = 0; y < height; ++y) {
        auto srcPtr = reinterpret_cast<const uint16_t *>(srcData);
        auto dstPtr = reinterpret_cast<uint16_t *>(data64Ptr);
        for (int x = 0; x < width; ++x) {
            auto alpha = half_to_float(srcPtr[3]);
            auto tmpR = (uint16_t) std::min(std::max((half_to_float(srcPtr[0]) / scale), 0.0f), maxColors);
            auto tmpG = (uint16_t) std::min(std::max((half_to_float(srcPtr[1]) / scale), 0.0f), maxColors);
            auto tmpB = (uint16_t) std::min(std::max((half_to_float(srcPtr[2]) / scale), 0.0f), maxColors);
            auto tmpA = (uint16_t) std::min(std::max((alpha / scale), 0.0f), maxColors);

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

void
RGBAF16BitToNBitU16(const uint16_t *sourceData, int srcStride, uint16_t *dst, int dstStride,
                    int width,
                    int height, int bitDepth) {
#if HAVE_NEON
    RGBAF16BitToNU16_NEON(sourceData, srcStride, dst, dstStride, width, height, bitDepth);
#else
    RGBAF16BitToNU16_C(sourceData, srcStride, dst, dstStride, width, height, bitDepth);
#endif
}