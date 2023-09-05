//
// Created by Radzivon Bartoshyk on 05/09/2023.
//

#include "rgb1010102.h"
#include <vector>

void convertRGBA1010102ToU16_C(const uint8_t *src, int srcStride, uint16_t *dst, int dstStride,
                               int width, int height) {
    auto mDstPointer = reinterpret_cast<uint8_t *>(dst);
    auto mSrcPointer = reinterpret_cast<const uint8_t *>(src);

    uint32_t testValue = 0x01020304;
    auto testBytes = reinterpret_cast<uint8_t *>(&testValue);

    bool littleEndian = false;
    if (testBytes[0] == 0x04) {
        littleEndian = true;
    } else if (testBytes[0] == 0x01) {
        littleEndian = false;
    }

    for (int y = 0; y < height; ++y) {

        auto dstPointer = reinterpret_cast<uint8_t *>(mDstPointer);
        auto srcPointer = reinterpret_cast<const uint8_t *>(mSrcPointer);

        for (int x = 0; x < width; ++x) {
            uint32_t rgba1010102 = reinterpret_cast<const uint32_t *>(srcPointer)[0];

            const uint32_t mask = (1u << 10u) - 1u;
            uint32_t b = (rgba1010102) & mask;
            uint32_t g = (rgba1010102 >> 10) & mask;
            uint32_t r = (rgba1010102 >> 20) & mask;

            uint32_t a1 = (rgba1010102 >> 30);
            uint32_t a = (a1 << 8) | (a1 << 6) | (a1 << 4) | (a1 << 2) | a1;

            // Convert each channel to floating-point values
            auto rFloat = static_cast<uint16_t>(r);
            auto gFloat = static_cast<uint16_t>(g);
            auto bFloat = static_cast<uint16_t>(b);
            auto aFloat = static_cast<uint16_t>(a);

            auto dstCast = reinterpret_cast<uint16_t *>(dstPointer);
            if (littleEndian) {
                dstCast[0] = bFloat;
                dstCast[1] = gFloat;
                dstCast[2] = rFloat;
                dstCast[3] = aFloat;
            } else {
                dstCast[0] = rFloat;
                dstCast[1] = gFloat;
                dstCast[2] = bFloat;
                dstCast[3] = aFloat;
            }

            srcPointer += 4;
            dstPointer += 8;
        }

        mSrcPointer += srcStride;
        mDstPointer += dstStride;
    }
}

void convertRGBA1010102ToU8_C(const uint8_t *src, int srcStride, uint8_t *dst, int dstStride,
                              int width, int height) {
    auto mDstPointer = reinterpret_cast<uint8_t *>(dst);
    auto mSrcPointer = reinterpret_cast<const uint8_t *>(src);

    uint32_t testValue = 0x01020304;
    auto testBytes = reinterpret_cast<uint8_t *>(&testValue);

    bool littleEndian = false;
    if (testBytes[0] == 0x04) {
        littleEndian = true;
    } else if (testBytes[0] == 0x01) {
        littleEndian = false;
    }

    for (int y = 0; y < height; ++y) {

        auto dstPointer = reinterpret_cast<uint8_t *>(mDstPointer);
        auto srcPointer = reinterpret_cast<const uint8_t *>(mSrcPointer);

        for (int x = 0; x < width; ++x) {
            uint32_t rgba1010102 = reinterpret_cast<const uint32_t *>(srcPointer)[0];

            const uint32_t mask = (1u << 10u) - 1u;
            uint32_t b = (rgba1010102) & mask;
            uint32_t g = (rgba1010102 >> 10) & mask;
            uint32_t r = (rgba1010102 >> 20) & mask;

            uint32_t a1 = (rgba1010102 >> 30);
            uint32_t a = (a1 << 8) | (a1 << 6) | (a1 << 4) | (a1 << 2) | a1;

            // Convert each channel to floating-point values
            auto rFloat = std::max(std::min(static_cast<uint8_t>((r * 255) / 1023), (uint8_t) 255),
                                   (uint8_t) 0);
            auto gFloat = std::max(std::min(static_cast<uint8_t>((g * 255) / 1023), (uint8_t) 255),
                                   (uint8_t) 0);
            auto bFloat = std::max(std::min(static_cast<uint8_t>((b * 255) / 1023), (uint8_t) 255),
                                   (uint8_t) 0);
            auto aFloat = std::max(std::min(static_cast<uint8_t>((a * 255) / 1023), (uint8_t) 255),
                                   (uint8_t) 0);

            auto dstCast = reinterpret_cast<uint8_t *>(dstPointer);
            if (littleEndian) {
                dstCast[0] = bFloat;
                dstCast[1] = gFloat;
                dstCast[2] = rFloat;
                dstCast[3] = aFloat;
            } else {
                dstCast[0] = rFloat;
                dstCast[1] = gFloat;
                dstCast[2] = bFloat;
                dstCast[3] = aFloat;
            }

            srcPointer += 4;
            dstPointer += 4;
        }

        mSrcPointer += srcStride;
        mDstPointer += dstStride;
    }
}

void RGBA1010102ToU8(const uint8_t *src, int srcStride, uint8_t *dst, int dstStride,
                     int width, int height) {
    convertRGBA1010102ToU8_C(src, srcStride, dst, dstStride, width, height);
}

void RGBA1010102ToU16(const uint8_t *src, int srcStride, uint16_t *dst, int dstStride,
                      int width, int height) {
    convertRGBA1010102ToU16_C(src, srcStride, dst, dstStride, width, height);
}