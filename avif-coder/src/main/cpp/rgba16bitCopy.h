//
// Created by Radzivon Bartoshyk on 05/09/2023.
//

#ifndef AVIF_RGBA16BITCOPY_H
#define AVIF_RGBA16BITCOPY_H

#include <vector>

void copyRGBA16(uint16_t* source, int srcStride,
                uint16_t *destination, int dstStride,
                int width, int height);

#endif //AVIF_RGBA16BITCOPY_H
