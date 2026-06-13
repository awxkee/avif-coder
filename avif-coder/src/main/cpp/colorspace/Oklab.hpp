//
// Created by Radzivon Bartoshyk on 10/11/2024.
//

#ifndef AVIF_OKLAB
#define AVIF_OKLAB

#include <cmath>

namespace coder {
class Rgb {
 public:
  float r, g, b;
  Rgb(float r_, float g_, float b_) : r(r_), g(g_), b(b_) {}

  Rgb operator+(const Rgb& other) const {
    return {r + other.r, g + other.g, b + other.b};
  }
  Rgb operator-(const Rgb& other) const {
    return {r - other.r, g - other.g, b - other.b};
  }
  Rgb operator*(const Rgb& other) const {
    return {r * other.r, g * other.g, b * other.b};
  }

  Rgb operator/(float scalar) const {
    return {r / scalar, g / scalar, b / scalar};
  }

  Rgb operator+(float scalar) const {
    return {r + scalar, g + scalar, b + scalar};
  }

  Rgb operator*(float scalar) const {
    return {r * scalar, g * scalar, b * scalar};
  }

  friend Rgb operator*(float scalar, const Rgb& color) {
    return {color.r * scalar, color.g * scalar, color.b * scalar};
  }

  Rgb operator-(float scalar) const {
    return {r - scalar, g - scalar, b - scalar};
  }
  Rgb operator/(const Rgb& other) const {
    return {r / other.r, g / other.g, b / other.b};
  }
};
}

#endif //AVIF_OKLAB
