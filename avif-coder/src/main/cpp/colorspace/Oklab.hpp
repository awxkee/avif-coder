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

class Oklab {
 public:
  // Public member variables for Oklab components
  float L, a, b;

  // Constructor to initialize Oklab color components
  constexpr Oklab(float l = 0.0f, float a_ = 0.0f, float b_ = 0.0f)
      : L(l), a(a_), b(b_) {}

  [[nodiscard]] static constexpr Oklab fromLinearRGB(float r, float g, float rgb_b) {
    float l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * rgb_b;
    float m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * rgb_b;
    float s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * rgb_b;

    float l_ = std::cbrtf(l);
    float m_ = std::cbrtf(m);
    float s_ = std::cbrtf(s);

    float oklab_l = 0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_;
    float oklab_a = 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_;
    float oklab_b = 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_;

    return {oklab_l, oklab_a, oklab_b};
  }

  [[nodiscard]] Rgb toLinearRGB() const {
    float l_ = this->L + 0.3963377774f * this->a + 0.2158037573f * this->b;
    float m_ = this->L - 0.1055613458f * this->a - 0.0638541728f * this->b;
    float s_ = this->L - 0.0894841775f * this->a - 1.2914855480f * this->b;

    float l = l_ * l_ * l_;
    float m = m_ * m_ * m_;
    float s = s_ * s_ * s_;

    float linear_r = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    float linear_g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    float linear_b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;

    return {linear_r, linear_g, linear_b};
  }
};
}

#endif //AVIF_OKLAB
