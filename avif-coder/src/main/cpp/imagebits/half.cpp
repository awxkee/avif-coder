//
// Created by Radzivon Bartoshyk on 20/10/2023.
//
#include "half.hpp"

float LoadHalf(uint16_t f) {
    half_float::half h;
    h.data_ = f;
    return (float)h;
}