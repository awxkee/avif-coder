//
// Created by Radzivon Bartoshyk on 16/09/2023.
//

#include "ReformatBitmap.h"
#include "imagebits/RgbaF16bitNBitU8.h"
#include "imagebits/Rgba8ToF16.h"
#include "imagebits/Rgb565.h"
#include "imagebits/Rgb1010102.h"
#include <string>
#include <android/bitmap.h>

void
ReformatColorConfig(std::shared_ptr<uint8_t> &imageData, std::string &imageConfig,
                    PreferredColorConfig preferredColorConfig, int depth,
                    int imageWidth, int imageHeight, int *stride, bool *useFloats) {

    switch (preferredColorConfig) {
        case Rgba_8888:
            if (*useFloats) {
                int dstStride = imageWidth * 4 * (int) sizeof(uint8_t);
                std::shared_ptr<uint8_t> rgba8888Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::RGBAF16BitToNBitU8(reinterpret_cast<const uint16_t *>(imageData.get()),
                                          *stride, rgba8888Data.get(), dstStride, imageWidth,
                                          imageHeight, 8);
                *stride = dstStride;
                *useFloats = false;
                imageConfig = "ARGB_8888";
                imageData = rgba8888Data;
            }
            break;
        case Rgba_F16:
            if (*useFloats) {
                break;
            } else {
                int dstStride = imageWidth * 4 * (int) sizeof(uint16_t);
                std::shared_ptr<uint8_t> rgbaF16Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::Rgba8ToF16(imageData.get(), *stride,
                                  reinterpret_cast<uint16_t *>(rgbaF16Data.get()), dstStride,
                                  imageWidth, imageHeight, depth);
                *stride = dstStride;
                *useFloats = true;
                imageConfig = "RGBA_F16";
                imageData = rgbaF16Data;
            }
            break;
        case Rgb_565:
            if (*useFloats) {
                int dstStride = imageWidth * (int) sizeof(uint16_t);
                std::shared_ptr<uint8_t> rgb565Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::RGBAF16To565(reinterpret_cast<const uint16_t *>(imageData.get()), *stride,
                                    reinterpret_cast<uint16_t *>(rgb565Data.get()), dstStride,
                                    imageWidth, imageHeight);
                *stride = dstStride;
                *useFloats = false;
                imageConfig = "RGB_565";
                imageData = rgb565Data;
                break;
            } else {
                int dstStride = imageWidth * (int) sizeof(uint16_t);
                std::shared_ptr<uint8_t> rgb565Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::Rgba8To565(imageData.get(), *stride,
                                  reinterpret_cast<uint16_t *>(rgb565Data.get()), dstStride,
                                  imageWidth, imageHeight, depth);
                *stride = dstStride;
                *useFloats = false;
                imageConfig = "RGB_565";
                imageData = rgb565Data;
            }
            break;
        case Rgba_1010102:
            if (*useFloats) {
                int dstStride = imageWidth * 4 * (int) sizeof(uint8_t);
                std::shared_ptr<uint8_t> rgba1010102Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::F16ToRGBA1010102(reinterpret_cast<const uint16_t *>(imageData.get()),
                                        *stride,
                                        reinterpret_cast<uint8_t *>(rgba1010102Data.get()),
                                        dstStride,
                                        imageWidth, imageHeight);
                *stride = dstStride;
                *useFloats = false;
                imageConfig = "RGBA_1010102";
                imageData = rgba1010102Data;
                break;
            } else {
                int dstStride = imageWidth * 4 * (int) sizeof(uint8_t);
                std::shared_ptr<uint8_t> rgba1010102Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::Rgba8ToRGBA1010102(reinterpret_cast<const uint8_t *>(imageData.get()),
                                          *stride,
                                          reinterpret_cast<uint8_t *>(rgba1010102Data.get()),
                                          dstStride,
                                          imageWidth, imageHeight);
                *stride = dstStride;
                *useFloats = false;
                imageConfig = "RGBA_1010102";
                imageData = rgba1010102Data;
                break;
            }
            break;
        default:
            break;
    }
}