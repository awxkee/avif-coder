//
// Created by Radzivon Bartoshyk on 03/09/2023.
//

#ifndef AVIF_ATTENUATE_ALPHA_H
#define AVIF_ATTENUATE_ALPHA_H

#include <cstdint>

void premultiply_alpha64(uint16_t *rgba_data, int num_pixels);
#if HAVE_NEON
void premultiply_alpha_NEON(uint16_t *rgba_data, int numPixels);
#endif

#endif //AVIF_ATTENUATE_ALPHA_H
