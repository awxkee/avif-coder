//
// Created by Radzivon Bartoshyk on 15/09/2023.
//

#include "Rgb565.h"
#include "ThreadPool.hpp"
#include "HalfFloats.h"
#include <algorithm>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "imagebits/Rgb565.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

    using hwy::HWY_NAMESPACE::Set;
    using hwy::HWY_NAMESPACE::FixedTag;
    using hwy::HWY_NAMESPACE::Vec;
    using hwy::HWY_NAMESPACE::Mul;
    using hwy::HWY_NAMESPACE::Max;
    using hwy::HWY_NAMESPACE::Min;
    using hwy::HWY_NAMESPACE::Zero;
    using hwy::HWY_NAMESPACE::BitCast;
    using hwy::HWY_NAMESPACE::ConvertTo;
    using hwy::HWY_NAMESPACE::PromoteTo;
    using hwy::HWY_NAMESPACE::DemoteTo;
    using hwy::HWY_NAMESPACE::Combine;
    using hwy::HWY_NAMESPACE::Rebind;
    using hwy::HWY_NAMESPACE::LowerHalf;
    using hwy::HWY_NAMESPACE::ShiftLeft;
    using hwy::HWY_NAMESPACE::ShiftRight;
    using hwy::HWY_NAMESPACE::UpperHalf;
    using hwy::HWY_NAMESPACE::LoadInterleaved4;
    using hwy::HWY_NAMESPACE::Store;
    using hwy::HWY_NAMESPACE::Or;
    using hwy::float16_t;

    void
    Rgba8To565HWYRow(const uint8_t *source, uint16_t *destination, int width) {
        const FixedTag<uint16_t, 8> du16;
        const FixedTag<uint8_t, 8> du8x8;
        using VU16 = Vec<decltype(du16)>;
        using VU8x8 = Vec<decltype(du8x8)>;

        Rebind<uint16_t, FixedTag<uint8_t, 8>> rdu16;

        int x = 0;
        int pixels = 8;

        auto src = reinterpret_cast<const uint8_t *>(source);
        auto dst = reinterpret_cast<uint16_t *>(destination);
        for (x = 0; x + pixels < width; x += pixels) {
            VU8x8 ru8Row;
            VU8x8 gu8Row;
            VU8x8 bu8Row;
            VU8x8 au8Row;
            LoadInterleaved4(du8x8, reinterpret_cast<const uint8_t *>(src),
                             ru8Row, gu8Row, bu8Row, au8Row);

            auto rdu16Vec = ShiftLeft<11>(ShiftRight<3>(PromoteTo(rdu16, ru8Row)));
            auto gdu16Vec = ShiftLeft<5>(ShiftRight<2>(PromoteTo(rdu16, gu8Row)));
            auto bdu16Vec = ShiftRight<3>(PromoteTo(rdu16, bu8Row));

            auto result = Or(Or(rdu16Vec, gdu16Vec), bdu16Vec);
            Store(result, du16, dst);
            src += 4 * pixels;
            dst += pixels;
        }

        for (; x < width; ++x) {
            uint16_t red565 = (src[0] >> 3) << 11;
            uint16_t green565 = (src[1] >> 2) << 5;
            uint16_t blue565 = src[2] >> 3;

            auto result = static_cast<uint16_t>(red565 | green565 | blue565);
            dst[0] = result;

            src += 4;
            dst += 1;
        }

    }

    void Rgba8To565HWY(const uint8_t *sourceData, int srcStride,
                       uint16_t *dst, int dstStride, int width,
                       int height, int bitDepth) {

        ThreadPool pool;
        std::vector<std::future<void>> results;

        auto mSrc = reinterpret_cast<const uint8_t *>(sourceData);
        auto mDst = reinterpret_cast<uint8_t *>(dst);

        for (int y = 0; y < height; ++y) {
            auto r = pool.enqueue(Rgba8To565HWYRow,
                                  reinterpret_cast<const uint8_t *>(mSrc),
                                  reinterpret_cast<uint16_t *>(mDst), width);
            results.push_back(std::move(r));
            mSrc += srcStride;
            mDst += dstStride;
        }

        for (auto &result: results) {
            result.wait();
        }

    }

    inline Vec<FixedTag<uint8_t, 8>>
    ConvertF16ToU16Row(Vec<FixedTag<uint16_t, 8>> v, float maxColors) {
        FixedTag<float16_t, 4> df16;
        FixedTag<uint16_t, 4> dfu416;
        FixedTag<uint8_t, 8> du8;
        Rebind<float, decltype(df16)> rf32;
        Rebind<int32_t, decltype(rf32)> ri32;
        Rebind<uint8_t, decltype(rf32)> ru8;

        using VU8 = Vec<decltype(du8)>;

        auto minColors = Zero(rf32);
        auto vMaxColors = Set(rf32, (int) maxColors);

        auto lower = DemoteTo(ru8, ConvertTo(ri32,
                                             Max(Min(Mul(
                                                     PromoteTo(rf32, BitCast(df16, LowerHalf(v))),
                                                     vMaxColors), vMaxColors), minColors)
        ));
        auto upper = DemoteTo(ru8, ConvertTo(ri32,
                                             Max(Min(Mul(PromoteTo(rf32,
                                                                   BitCast(df16,
                                                                           UpperHalf(dfu416, v))),
                                                         vMaxColors), vMaxColors), minColors)
        ));
        return Combine(du8, upper, lower);
    }

    void
    RGBAF16To565RowHWY(const uint16_t *source, uint16_t *destination, int width,
                       float maxColors) {
        const FixedTag<uint16_t, 8> du16;
        const FixedTag<uint8_t, 8> du8;
        using VU16 = Vec<decltype(du16)>;
        using VU8 = Vec<decltype(du8)>;

        Rebind<uint16_t, FixedTag<uint8_t, 8>> rdu16;

        int x = 0;
        int pixels = 8;

        auto src = reinterpret_cast<const uint16_t *>(source);
        auto dst = reinterpret_cast<uint16_t *>(destination);
        for (x = 0; x + pixels < width; x += pixels) {
            VU16 ru16Row;
            VU16 gu16Row;
            VU16 bu16Row;
            VU16 au16Row;
            LoadInterleaved4(du16, reinterpret_cast<const uint16_t *>(src),
                             ru16Row, gu16Row,
                             bu16Row,
                             au16Row);

            auto r16Row = ConvertF16ToU16Row(ru16Row, maxColors);
            auto g16Row = ConvertF16ToU16Row(gu16Row, maxColors);
            auto b16Row = ConvertF16ToU16Row(bu16Row, maxColors);

            auto rdu16Vec = ShiftLeft<11>(ShiftRight<3>(PromoteTo(rdu16, r16Row)));
            auto gdu16Vec = ShiftLeft<5>(ShiftRight<2>(PromoteTo(rdu16, g16Row)));
            auto bdu16Vec = ShiftRight<3>(PromoteTo(rdu16, b16Row));

            auto result = Or(Or(rdu16Vec, gdu16Vec), bdu16Vec);
            Store(result, du16, dst);

            src += 4 * pixels;
            dst += pixels;
        }

        for (; x < width; ++x) {
            uint16_t red565 = ((uint16_t )roundf(half_to_float(src[0]) * maxColors) >> 3) << 11;
            uint16_t green565 = ((uint16_t )roundf(half_to_float(src[1]) * maxColors) >> 2) << 5;
            uint16_t blue565 = (uint16_t )roundf(half_to_float(src[2]) * maxColors) >> 3;

            auto result = static_cast<uint16_t>(red565 | green565 | blue565);
            dst[0] = result;

            src += 4;
            dst += 1;
        }

    }

    void RGBAF16To565HWY(const uint16_t *sourceData, int srcStride,
                         uint16_t *dst, int dstStride, int width,
                         int height) {
        ThreadPool pool;
        std::vector<std::future<void>> results;

        float maxColors = powf(2, (float) 8) - 1;

        auto mSrc = reinterpret_cast<const uint8_t *>(sourceData);
        auto mDst = reinterpret_cast<uint8_t *>(dst);

        for (int y = 0; y < height; ++y) {
            auto r = pool.enqueue(RGBAF16To565RowHWY,
                                  reinterpret_cast<const uint16_t *>(mSrc),
                                  reinterpret_cast<uint16_t *>(mDst), width,
                                  maxColors);
            results.push_back(std::move(r));
            mSrc += srcStride;
            mDst += dstStride;
        }

        for (auto &result: results) {
            result.wait();
        }
    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(Rgba8To565HWY);
    HWY_DLLEXPORT void Rgba8To565(const uint8_t *sourceData, int srcStride,
                                  uint16_t *dst, int dstStride, int width,
                                  int height, int bitDepth) {
        HWY_DYNAMIC_DISPATCH(Rgba8To565HWY)(sourceData, srcStride, dst, dstStride, width,
                                            height, bitDepth);
    }

    HWY_EXPORT(RGBAF16To565HWY);
    HWY_DLLEXPORT void RGBAF16To565(const uint16_t *sourceData, int srcStride,
                                    uint16_t *dst, int dstStride, int width,
                                    int height) {
        HWY_DYNAMIC_DISPATCH(RGBAF16To565HWY)(sourceData, srcStride, dst, dstStride, width,
                                              height);
    }
}
#endif