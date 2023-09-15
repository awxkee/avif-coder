//
// Created by Radzivon Bartoshyk on 05/09/2023.
//

#include "Rgb1010102.h"
#include <vector>
#include "ThreadPool.hpp"
#include <algorithm>
#include "HalfFloats.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "Rgb1010102.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

#if HAVE_NEON

#include <arm_neon.h>

void convertRGBA1010102ToU8_NEON(const uint8_t *src, int srcStride, uint8_t *dst, int dstStride,
                                 int width, int height) {
    uint32x4_t mask = vdupq_n_u32(0x3FF); // Create a vector with 10 bits set
    auto maxColors = vdupq_n_u32(255);
    auto minColors = vdupq_n_u32(0);

    uint32_t testValue = 0x01020304;
    auto testBytes = reinterpret_cast<uint8_t *>(&testValue);

    bool littleEndian = false;
    if (testBytes[0] == 0x04) {
        littleEndian = true;
    } else if (testBytes[0] == 0x01) {
        littleEndian = false;
    }

    auto dstPtr = reinterpret_cast<uint8_t *>(dst);

    auto m8Bit = vdupq_n_u32(255);
    const uint32_t scalarMask = (1u << 10u) - 1u;

    for (int y = 0; y < height; ++y) {
        const uint8_t *srcPointer = src;
        uint8_t *dstPointer = dstPtr;
        int x;
        for (x = 0; x + 4 < width; x += 4) {
            uint32x4_t rgba1010102 = vld1q_u32(reinterpret_cast<const uint32_t *>(srcPointer));

            auto originalR = vshrq_n_u32(
                    vmulq_u32(vandq_u32(vshrq_n_u32(rgba1010102, 20), mask), m8Bit), 10);
            uint32x4_t r = vmaxq_u32(vminq_u32(originalR, maxColors), minColors);
            auto originalG = vshrq_n_u32(
                    vmulq_u32(vandq_u32(vshrq_n_u32(rgba1010102, 10), mask), m8Bit), 10);
            uint32x4_t g = vmaxq_u32(vminq_u32(originalG, maxColors), minColors);
            auto originalB = vshrq_n_u32(vmulq_u32(vandq_u32(rgba1010102, mask), m8Bit), 10);
            uint32x4_t b = vmaxq_u32(vminq_u32(originalB, maxColors), minColors);

            uint32x4_t a1 = vshrq_n_u32(rgba1010102, 30);

            uint32x4_t aq = vorrq_u32(vshlq_n_u32(a1, 8), vorrq_u32(vshlq_n_u32(a1, 6),
                                                                    vorrq_u32(vshlq_n_u32(a1, 4),
                                                                              vorrq_u32(
                                                                                      vshlq_n_u32(
                                                                                              a1,
                                                                                              2),
                                                                                      a1))));
            uint32x4_t a = vshrq_n_u32(vmulq_u32(aq, m8Bit), 10);

            uint8x8_t rUInt8 = vqmovn_u16(vqmovn_high_u32(vqmovn_u32(r), r));
            uint8x8_t gUInt8 = vqmovn_u16(vqmovn_high_u32(vqmovn_u32(g), g));
            uint8x8_t bUInt8 = vqmovn_u16(vqmovn_high_u32(vqmovn_u32(b), b));
            uint8x8_t aUInt8 = vqmovn_u16(vqmovn_high_u32(vqmovn_u32(a), a));

            uint8x8x4_t resultRGBA;
            // Interleave the channels to get RGBA format
            resultRGBA.val[0] = vzip_u8(bUInt8, bUInt8).val[0];
            resultRGBA.val[1] = vzip_u8(gUInt8, gUInt8).val[0];
            resultRGBA.val[2] = vzip_u8(rUInt8, rUInt8).val[1];
            resultRGBA.val[3] = vzip_u8(aUInt8, aUInt8).val[1];

            vst4_u8(reinterpret_cast<uint8_t *>(dstPointer), resultRGBA);

            srcPointer += 16;
            dstPointer += 16;
        }

        for (; x < width; ++x) {
            uint32_t rgba1010102 = reinterpret_cast<const uint32_t *>(srcPointer)[0];

            uint32_t b = (rgba1010102) & scalarMask;
            uint32_t g = (rgba1010102 >> 10) & scalarMask;
            uint32_t r = (rgba1010102 >> 20) & scalarMask;

            uint32_t a1 = (rgba1010102 >> 30);
            uint32_t a = (a1 << 8) | (a1 << 6) | (a1 << 4) | (a1 << 2) | a1;

            // Convert each channel to floating-point values
            auto rUInt16 = static_cast<uint16_t>(r);
            auto gUInt16 = static_cast<uint16_t>(g);
            auto bUInt16 = static_cast<uint16_t>(b);
            auto aUInt16 = static_cast<uint16_t>(a);

            auto dstCast = reinterpret_cast<uint16_t *>(dstPointer);

            if (littleEndian) {
                dstCast[0] = bUInt16;
                dstCast[1] = gUInt16;
                dstCast[2] = rUInt16;
                dstCast[3] = aUInt16;
            } else {
                dstCast[0] = rUInt16;
                dstCast[1] = gUInt16;
                dstCast[2] = bUInt16;
                dstCast[3] = aUInt16;
            }

            srcPointer += 4;
            dstPointer += 4;
        }

        src += srcStride;
        dstPtr += dstStride;
    }
}

void convertRGBA1010102ToU16_NEON(const uint8_t *src, int srcStride, uint16_t *dst, int dstStride,
                                  int width, int height) {
    uint32x4_t mask = vdupq_n_u32(0x3FF); // Create a vector with 10 bits set
    auto maxColors = vdupq_n_u32(1023);
    auto minColors = vdupq_n_u32(0);

    uint32_t testValue = 0x01020304;
    auto testBytes = reinterpret_cast<uint8_t *>(&testValue);

    bool littleEndian = false;
    if (testBytes[0] == 0x04) {
        littleEndian = true;
    } else if (testBytes[0] == 0x01) {
        littleEndian = false;
    }

    const uint32_t scalarMask = (1u << 10u) - 1u;

    auto dstPtr = reinterpret_cast<uint8_t *>(dst);

    for (int y = 0; y < height; ++y) {
        const uint8_t *srcPointer = src;
        uint8_t *dstPointer = dstPtr;
        int x;
        for (x = 0; x + 2 < width; x += 2) {
            uint32x4_t rgba1010102 = vld1q_u32(reinterpret_cast<const uint32_t *>(srcPointer));

            uint32x4_t r = vmaxq_u32(
                    vminq_u32(vandq_u32(vshrq_n_u32(rgba1010102, 20), mask), maxColors),
                    minColors);
            uint32x4_t g = vmaxq_u32(
                    vminq_u32(vandq_u32(vshrq_n_u32(rgba1010102, 10), mask), maxColors),
                    minColors);
            uint32x4_t b = vmaxq_u32(vminq_u32(vandq_u32(rgba1010102, mask), maxColors),
                                     minColors);

            uint32x4_t a1 = vshrq_n_u32(rgba1010102, 30);

            uint32x4_t a = vorrq_u32(vshlq_n_u32(a1, 8), vorrq_u32(vshlq_n_u32(a1, 6),
                                                                   vorrq_u32(vshlq_n_u32(a1, 4),
                                                                             vorrq_u32(
                                                                                     vshlq_n_u32(a1,
                                                                                                 2),
                                                                                     a1))));

            uint16x4_t rUInt16 = vqmovn_u32(b);
            uint16x4_t gUInt16 = vqmovn_u32(g);
            uint16x4_t bUInt16 = vqmovn_u32(r);
            uint16x4_t aUInt16 = vqmovn_u32(vmaxq_u32(vminq_u32(a, maxColors), minColors));

            uint16x4x4_t interleavedRGBA;
            interleavedRGBA.val[0] = rUInt16;
            interleavedRGBA.val[1] = gUInt16;
            interleavedRGBA.val[2] = bUInt16;
            interleavedRGBA.val[3] = aUInt16;

            vst4_u16(reinterpret_cast<uint16_t *>(dstPointer), interleavedRGBA);

            srcPointer += 8;
            dstPointer += 16;
        }

        for (; x < width; ++x) {
            uint32_t rgba1010102 = reinterpret_cast<const uint32_t *>(srcPointer)[0];
            uint32_t b = (rgba1010102) & scalarMask;
            uint32_t g = (rgba1010102 >> 10) & scalarMask;
            uint32_t r = (rgba1010102 >> 20) & scalarMask;

            uint32_t a1 = (rgba1010102 >> 30);
            uint32_t a = (a1 << 8) | (a1 << 6) | (a1 << 4) | (a1 << 2) | a1;

            // Convert each channel to floating-point values
            auto rFloat = std::clamp(static_cast<uint8_t>((r * 255) / 1023), (uint8_t) 255,
                                     (uint8_t) 0);
            auto gFloat = std::clamp(static_cast<uint8_t>((g * 255) / 1023), (uint8_t) 255,
                                     (uint8_t) 0);
            auto bFloat = std::clamp(static_cast<uint8_t>((b * 255) / 1023), (uint8_t) 255,
                                     (uint8_t) 0);
            auto aFloat = std::clamp(static_cast<uint8_t>((a * 255) / 1023), (uint8_t) 255,
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


        src += srcStride;
        dstPtr += dstStride;
    }
}

#endif

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
            auto rUInt16 = static_cast<uint16_t>(r);
            auto gUInt16 = static_cast<uint16_t>(g);
            auto bUInt16 = static_cast<uint16_t>(b);
            auto aUInt16 = static_cast<uint16_t>(a);

            auto dstCast = reinterpret_cast<uint16_t *>(dstPointer);

            if (littleEndian) {
                dstCast[0] = bUInt16;
                dstCast[1] = gUInt16;
                dstCast[2] = rUInt16;
                dstCast[3] = aUInt16;
            } else {
                dstCast[0] = rUInt16;
                dstCast[1] = gUInt16;
                dstCast[2] = bUInt16;
                dstCast[3] = aUInt16;
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

    const uint32_t mask = (1u << 10u) - 1u;

    for (int y = 0; y < height; ++y) {

        auto dstPointer = reinterpret_cast<uint8_t *>(mDstPointer);
        auto srcPointer = reinterpret_cast<const uint8_t *>(mSrcPointer);

        for (int x = 0; x < width; ++x) {
            uint32_t rgba1010102 = reinterpret_cast<const uint32_t *>(srcPointer)[0];

            uint32_t b = (rgba1010102) & mask;
            uint32_t g = (rgba1010102 >> 10) & mask;
            uint32_t r = (rgba1010102 >> 20) & mask;

            uint32_t a1 = (rgba1010102 >> 30);
            uint32_t a = (a1 << 8) | (a1 << 6) | (a1 << 4) | (a1 << 2) | a1;

            // Convert each channel to floating-point values
            auto rFloat = std::clamp(static_cast<uint8_t>((r * 255) / 1023), (uint8_t) 255,
                                     (uint8_t) 0);
            auto gFloat = std::clamp(static_cast<uint8_t>((g * 255) / 1023), (uint8_t) 255,
                                     (uint8_t) 0);
            auto bFloat = std::clamp(static_cast<uint8_t>((b * 255) / 1023), (uint8_t) 255,
                                     (uint8_t) 0);
            auto aFloat = std::clamp(static_cast<uint8_t>((a * 255) / 1023), (uint8_t) 255,
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
#if HAVE_NEON
    convertRGBA1010102ToU8_NEON(src, srcStride, dst, dstStride, width, height);
#else
    convertRGBA1010102ToU8_C(src, srcStride, dst, dstStride, width, height);
#endif
}

void RGBA1010102ToU16(const uint8_t *src, int srcStride, uint16_t *dst, int dstStride,
                      int width, int height) {
#if HAVE_NEON
    convertRGBA1010102ToU16_NEON(src, srcStride, dst, dstStride, width, height);
#else
    convertRGBA1010102ToU16_C(src, srcStride, dst, dstStride, width, height);
#endif
}


HWY_BEFORE_NAMESPACE();

namespace coder {
    namespace HWY_NAMESPACE {

        using hwy::HWY_NAMESPACE::FixedTag;
        using hwy::HWY_NAMESPACE::Rebind;
        using hwy::HWY_NAMESPACE::Vec;
        using hwy::HWY_NAMESPACE::Set;
        using hwy::HWY_NAMESPACE::Zero;
        using hwy::HWY_NAMESPACE::Load;
        using hwy::HWY_NAMESPACE::Mul;
        using hwy::HWY_NAMESPACE::Max;
        using hwy::HWY_NAMESPACE::Min;
        using hwy::HWY_NAMESPACE::And;
        using hwy::HWY_NAMESPACE::Add;
        using hwy::HWY_NAMESPACE::BitCast;
        using hwy::HWY_NAMESPACE::ShiftRight;
        using hwy::HWY_NAMESPACE::ShiftLeft;
        using hwy::HWY_NAMESPACE::ExtractLane;
        using hwy::HWY_NAMESPACE::DemoteTo;
        using hwy::HWY_NAMESPACE::ConvertTo;
        using hwy::HWY_NAMESPACE::PromoteTo;
        using hwy::HWY_NAMESPACE::LoadInterleaved4;
        using hwy::HWY_NAMESPACE::Or;
        using hwy::HWY_NAMESPACE::ShiftLeft;
        using hwy::HWY_NAMESPACE::Store;

        void
        F16ToRGBA1010102HWYRow(const uint16_t *HWY_RESTRICT data, uint32_t *HWY_RESTRICT dst,
                               int width,
                               const int *permuteMap) {
            float range10 = powf(2, 10) - 1;
            const FixedTag<float, 4> df;
            const FixedTag<float16_t, 4> df16;
            const FixedTag<uint16_t, 4> du16;
            const Rebind<int32_t, FixedTag<float, 4>> di32;
            const FixedTag<uint32_t, 4> du;
            using V = Vec<decltype(df)>;
            using V16 = Vec<decltype(df16)>;
            using VU16 = Vec<decltype(du16)>;
            using VU = Vec<decltype(du)>;
            const auto vRange10 = Set(df, range10);
            const auto zeros = Zero(df);
            const auto alphaMax = Set(df, 3.0);

            const Rebind<float16_t, FixedTag<uint16_t, 4>> rbfu16;
            const Rebind<float, FixedTag<float16_t, 4>> rbf32;

            int x = 0;
            auto dst32 = reinterpret_cast<uint32_t *>(dst);
            int pixels = 4;
            for (x = 0; x + pixels < width; x += pixels) {
                VU16 upixels1;
                VU16 upixels2;
                VU16 upixels3;
                VU16 upixels4;
                LoadInterleaved4(du16, reinterpret_cast<const uint16_t *>(data), upixels1, upixels2,
                                 upixels3, upixels4);
                V pixels1 = Min(
                        Max(Mul(PromoteTo(rbf32, BitCast(rbfu16, upixels1)), vRange10), zeros),
                        vRange10);
                V pixels2 = Min(
                        Max(Mul(PromoteTo(rbf32, BitCast(rbfu16, upixels2)), vRange10), zeros),
                        vRange10);
                V pixels3 = Min(
                        Max(Mul(PromoteTo(rbf32, BitCast(rbfu16, upixels3)), vRange10), zeros),
                        vRange10);
                V pixels4 = Min(
                        Max(Mul(PromoteTo(rbf32, BitCast(rbfu16, upixels4)), alphaMax), zeros),
                        alphaMax);
                VU pixelsu1 = BitCast(du, ConvertTo(di32, pixels1));
                VU pixelsu2 = BitCast(du, ConvertTo(di32, pixels2));
                VU pixelsu3 = BitCast(du, ConvertTo(di32, pixels3));
                VU pixelsu4 = BitCast(du, ConvertTo(di32, pixels4));

                VU pixelsStore[4] = {pixelsu1, pixelsu2, pixelsu3, pixelsu4};
                VU AV = pixelsStore[permuteMap[0]];
                VU RV = pixelsStore[permuteMap[1]];
                VU GV = pixelsStore[permuteMap[2]];
                VU BV = pixelsStore[permuteMap[3]];
                VU upper = Or(ShiftLeft<30>(AV), ShiftLeft<20>(RV));
                VU lower = Or(ShiftLeft<10>(GV), BV);
                VU final = Or(upper, lower);
                Store(final, du, dst32);
                data += pixels * 4;
                dst32 += pixels;
            }

            for (; x < width; ++x) {
                auto A16 = (float) half_to_float(data[permuteMap[0]]);
                auto R16 = (float) half_to_float(data[permuteMap[1]]);
                auto G16 = (float) half_to_float(data[permuteMap[2]]);
                auto B16 = (float) half_to_float(data[permuteMap[3]]);
                auto R10 = (uint32_t) (std::clamp(R16 * range10, 0.0f, (float) range10));
                auto G10 = (uint32_t) (std::clamp(G16 * range10, 0.0f, (float) range10));
                auto B10 = (uint32_t) (std::clamp(B16 * range10, 0.0f, (float) range10));
                auto A10 = (uint32_t) std::clamp(roundf(A16 * 3.f), 0.0f, 3.0f);

                dst32[0] = (A10 << 30) | (R10 << 20) | (G10 << 10) | B10;

                data += 4;
                dst32 += 1;
            }
        }

        void
        Rgba8ToRGBA1010102HWYRow(const uint8_t *HWY_RESTRICT data, uint32_t *HWY_RESTRICT dst,
                                 int width,
                                 const int *permuteMap) {
            const FixedTag<uint8_t, 4> du8x4;
            const Rebind<int32_t, FixedTag<uint8_t, 4>> di32;
            const FixedTag<uint32_t, 4> du;
            using VU8 = Vec<decltype(du8x4)>;
            using VU = Vec<decltype(du)>;

            int x = 0;
            auto dst32 = reinterpret_cast<uint32_t *>(dst);
            int pixels = 4;
            for (x = 0; x + pixels < width; x += pixels) {
                VU8 upixels1;
                VU8 upixels2;
                VU8 upixels3;
                VU8 upixels4;
                LoadInterleaved4(du8x4, reinterpret_cast<const uint8_t *>(data), upixels1, upixels2,
                                 upixels3, upixels4);

                VU pixelsu1 = Mul(BitCast(du, PromoteTo(di32, upixels1)), Set(du, 4));
                VU pixelsu2 = Mul(BitCast(du, PromoteTo(di32, upixels2)), Set(du, 4));
                VU pixelsu3 = Mul(BitCast(du, PromoteTo(di32, upixels3)), Set(du, 4));
                VU pixelsu4 = ShiftRight<8>(
                        Add(Mul(BitCast(du, PromoteTo(di32, upixels4)), Set(du, 3)), Set(du, 127)));

                VU pixelsStore[4] = {pixelsu1, pixelsu2, pixelsu3, pixelsu4};
                VU AV = pixelsStore[permuteMap[0]];
                VU RV = pixelsStore[permuteMap[1]];
                VU GV = pixelsStore[permuteMap[2]];
                VU BV = pixelsStore[permuteMap[3]];
                VU upper = Or(ShiftLeft<30>(AV), ShiftLeft<20>(RV));
                VU lower = Or(ShiftLeft<10>(GV), BV);
                VU final = Or(upper, lower);
                Store(final, du, dst32);
                data += pixels * 4;
                dst32 += pixels;
            }

            for (; x < width; ++x) {
                auto A16 = ((uint32_t) data[permuteMap[0]] * 3 + 127) >> 8;
                auto R16 = (uint32_t) data[permuteMap[1]] * 4;
                auto G16 = (uint32_t) data[permuteMap[2]] * 4;
                auto B16 = (uint32_t) data[permuteMap[3]] * 4;

                dst32[0] = (A16 << 30) | (R16 << 20) | (G16 << 10) | B16;
                data += 4;
                dst32 += 1;
            }
        }

        void
        Rgba8ToRGBA1010102HWY(const uint8_t *source,
                              int srcStride,
                              uint8_t *destination,
                              int dstStride,
                              int width,
                              int height) {
            int permuteMap[4] = {3, 2, 1, 0};
            int minimumTilingAreaSize = 850 * 850;
            auto src = reinterpret_cast<const uint8_t *>(source);
            int currentAreaSize = width * height;
            if (minimumTilingAreaSize > currentAreaSize) {
                for (int y = 0; y < height; ++y) {
                    Rgba8ToRGBA1010102HWYRow(
                            reinterpret_cast<const uint8_t *>(src + srcStride * y),
                            reinterpret_cast<uint32_t *>(destination + dstStride * y),
                            width, &permuteMap[0]);
                }
            } else {
                ThreadPool pool;
                std::vector<std::future<void>> results;

                for (int y = 0; y < height; ++y) {
                    auto r = pool.enqueue(Rgba8ToRGBA1010102HWYRow,
                                          reinterpret_cast<const uint8_t *>(src +
                                                                            srcStride * y),
                                          reinterpret_cast<uint32_t *>(destination + dstStride * y),
                                          width, &permuteMap[0]);
                    results.push_back(std::move(r));
                }

                for (auto &result: results) {
                    result.wait();
                }

            }
        }

        void
        F16ToRGBA1010102HWY(const uint16_t *source,
                            int srcStride,
                            uint8_t *destination,
                            int dstStride,
                            int width,
                            int height) {
            int permuteMap[4] = {3, 2, 1, 0};
            int minimumTilingAreaSize = 850 * 850;
            auto src = reinterpret_cast<const uint8_t *>(source);
            int currentAreaSize = width * height;
            if (minimumTilingAreaSize > currentAreaSize) {
                for (int y = 0; y < height; ++y) {
                    F16ToRGBA1010102HWYRow(
                            reinterpret_cast<const uint16_t *>(src + srcStride * y),
                            reinterpret_cast<uint32_t *>(destination + dstStride * y),
                            width, &permuteMap[0]);
                }
            } else {
                ThreadPool pool;
                std::vector<std::future<void>> results;

                for (int y = 0; y < height; ++y) {
                    auto r = pool.enqueue(F16ToRGBA1010102HWYRow,
                                          reinterpret_cast<const uint16_t *>(src +
                                                                             srcStride * y),
                                          reinterpret_cast<uint32_t *>(destination + dstStride * y),
                                          width, &permuteMap[0]);
                    results.push_back(std::move(r));
                }

                for (auto &result: results) {
                    result.wait();
                }

            }
        }

// NOLINTNEXTLINE(google-readability-namespace-comments)
    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace coder {
    HWY_EXPORT(F16ToRGBA1010102HWY);
    HWY_EXPORT(Rgba8ToRGBA1010102HWY);
    HWY_DLLEXPORT void
    F16ToRGBA1010102(const uint16_t *source, int srcStride, uint8_t *destination, int dstStride,
                     int width,
                     int height) {
        HWY_DYNAMIC_DISPATCH(F16ToRGBA1010102HWY)(source, srcStride, destination, dstStride, width,
                                                  height);
    }

    HWY_DLLEXPORT void
    Rgba8ToRGBA1010102(const uint8_t *source,
                       int srcStride,
                       uint8_t *destination,
                       int dstStride,
                       int width,
                       int height) {
        HWY_DYNAMIC_DISPATCH(Rgba8ToRGBA1010102HWY)(source, srcStride, destination, dstStride,
                                                    width,
                                                    height);
    }
}

#endif