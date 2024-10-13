//
// Created by Radzivon Bartoshyk on 13/10/2024.
//

#ifndef AVIF_CODER_SRC_MAIN_CPP_IMAGEFRAME_H_
#define AVIF_CODER_SRC_MAIN_CPP_IMAGEFRAME_H_

#include <cstdint>
#include "definitions.h"

struct AvifImageSize {
  uint32_t width;
  uint32_t height;
};

struct AvifImageFrame {
  aligned_uint8_vector store;
  uint32_t width;
  uint32_t height;
  bool is16Bit;
  uint32_t bitDepth;
  bool hasAlpha;
};

#endif //AVIF_AVIF_CODER_SRC_MAIN_CPP_IMAGEFRAME_H_
