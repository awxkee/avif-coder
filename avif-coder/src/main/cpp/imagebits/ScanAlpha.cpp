//
// Created by Radzivon Bartoshyk on 12/10/2024.
//

#include "ScanAlpha.h"
#include <limits>

template<typename T>
bool isImageHasAlpha(T *image, uint32_t stride, uint32_t width, uint32_t height) {
  T firstItem = image[3];
  if (firstItem != std::numeric_limits<T>::max()) {
    return true;
  }
  for (uint32_t y = 0; y < height; ++y) {
    uint32_t sums = 0;
    auto lane = reinterpret_cast<T *>(reinterpret_cast<uint8_t *>(image) + y * stride);
    for (uint32_t x = 0; x < width; ++x) {
      sums += lane[3] ^ firstItem;
      lane += 4;
    }
    if (sums != 0) {
      return true;
    }
  }

  return false;
}

template bool isImageHasAlpha(uint8_t *image, uint32_t stride, uint32_t width, uint32_t height);
template bool isImageHasAlpha(uint16_t *image, uint32_t stride, uint32_t width, uint32_t height);