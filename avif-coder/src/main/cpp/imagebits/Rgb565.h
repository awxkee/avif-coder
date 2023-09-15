//
// Created by Radzivon Bartoshyk on 15/09/2023.
//

#ifndef AVIF_RGB565_H
#define AVIF_RGB565_H

#include <cstdint>

namespace coder {
    void Rgba8To565(const uint8_t *sourceData, int srcStride,
                    uint16_t *dst, int dstStride, int width,
                    int height, int bitDepth);

    void RGBAF16To565(const uint16_t *sourceData, int srcStride,
                      uint16_t *dst, int dstStride, int width,
                      int height);
}

#endif //AVIF_RGB565_H
