//
// Created by Radzivon Bartoshyk on 15/09/2023.
//

#ifndef AVIF_RGBA8TOF16_H
#define AVIF_RGBA8TOF16_H

#include <cstdint>

namespace coder {
    void Rgba8ToF16(const uint8_t *sourceData, int srcStride,
                    uint16_t *dst, int dstStride, int width,
                    int height, int bitDepth);
}

#endif //AVIF_RGBA8TOF16_H
