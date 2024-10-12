//
// Created by Radzivon Bartoshyk on 12/10/2024.
//

#ifndef AVIF_AVIF_CODER_SRC_MAIN_CPP_IMAGEBITS_RGBA16_H_
#define AVIF_AVIF_CODER_SRC_MAIN_CPP_IMAGEBITS_RGBA16_H_

#include <cstdint>

namespace coder {
void
Rgba16ToRgba8(const uint16_t *source,
              uint32_t srcStride,
              uint8_t *destination,
              uint32_t dstStride,
              uint32_t width,
              uint32_t height,
              uint32_t bitDepth);
}

#endif //AVIF_AVIF_CODER_SRC_MAIN_CPP_IMAGEBITS_RGBA16_H_
