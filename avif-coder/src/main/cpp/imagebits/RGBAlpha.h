//
// Created by Radzivon Bartoshyk on 06/11/2023.
//

#ifndef JXLCODER_RGBALPHA_H
#define JXLCODER_RGBALPHA_H

#include <cstdint>

namespace coder {
    void UnpremultiplyRGBA(const uint8_t *src, int srcStride,
                           uint8_t *dst, int dstStride, int width,
                           int height);

    void PremultiplyRGBA(const uint8_t *src, int srcStride,
                         uint8_t *dst, int dstStride, int width,
                         int height);
}

#endif //JXLCODER_RGBALPHA_H
