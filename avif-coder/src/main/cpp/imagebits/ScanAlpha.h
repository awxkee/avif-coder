//
// Created by Radzivon Bartoshyk on 12/10/2024.
//

#ifndef AVIF_AVIF_CODER_SRC_MAIN_CPP_IMAGEBITS_SCANALPHA_H_
#define AVIF_AVIF_CODER_SRC_MAIN_CPP_IMAGEBITS_SCANALPHA_H_

#include <cstdint>

template<typename T>
bool isImageHasAlpha(T* image, uint32_t stride, uint32_t width, uint32_t height);

#endif //AVIF_AVIF_CODER_SRC_MAIN_CPP_IMAGEBITS_SCANALPHA_H_
