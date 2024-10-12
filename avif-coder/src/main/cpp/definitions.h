//
// Created by Radzivon Bartoshyk on 11/10/2024.
//

#ifndef AVIF_DEFINITIONS_H
#define AVIF_DEFINITIONS_H

#include <aligned_allocator.h>
#include <vector>

typedef std::vector<float, aligned_allocator<float, sizeof(float)>> aligned_float_vector;
typedef std::vector<uint8_t, aligned_allocator<uint8_t, sizeof(uint32_t)>> aligned_uint8_vector;
typedef std::vector<uint16_t, aligned_allocator<uint16_t, sizeof(uint32_t)>> aligned_uint16_vector;

#endif //AVIF_DEFINITIONS_H
