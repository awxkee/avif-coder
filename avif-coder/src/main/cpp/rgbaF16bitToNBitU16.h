//
// Created by Radzivon Bartoshyk on 05/09/2023.
//

#ifndef AVIF_RGBAF16BITTONBITU16_H
#define AVIF_RGBAF16BITTONBITU16_H

#include <cstdint>
#include <vector>

void RGBAF16BitToNBitU16(const uint16_t *sourceData, int srcStride,
                         uint16_t *dst, int dstStride, int width,
                        int height, int bitDepth);

#endif //AVIF_RGBAF16BITTONBITU16_H
