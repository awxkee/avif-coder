//
// Created by Radzivon Bartoshyk on 03/09/2023.
//

#include "attenuate_alpha.h"

#if HAVE_NEON

#include <cstdint>
#include <arm_neon.h>

void premultiply_alpha_NEON(uint16_t *rgba_data, int numPixels) {
    // Process pixels in batches of 8 using NEON
    int batchSize = numPixels / 8;
    uint16x8_t alpha_mult, r, g, b;

    uint16_t *pixel = rgba_data;

    uint16x8_t neon_const_32768 = vdupq_n_u16(32768);

    auto pixels = rgba_data;
    for (int i = 0; i < batchSize; i += 8) {
        uint16x8_t alpha = vld1q_u16(pixels + 3); // Load alpha values
        uint16x8_t red = vld1q_u16(pixels + i);        // Load red values
        uint16x8_t green = vld1q_u16(pixels + 1);  // Load green values
        uint16x8_t blue = vld1q_u16(pixels + 2);   // Load blue values

        // Perform the premultiplication
        red = vmulq_u16(red, alpha);
        green = vmulq_u16(green, alpha);
        blue = vmulq_u16(blue, alpha);

        // Divide the result by 65536 (shift right by 16)
        red = vshrq_n_u16(red, 16);
        green = vshrq_n_u16(green, 16);
        blue = vshrq_n_u16(blue, 16);

        // Store the premultiplied values back to memory
        vst1q_u16(pixels + i, red);
        vst1q_u16(pixels + i + 1, green);
        vst1q_u16(pixels + i + 2, blue);

        pixels += 4;
    }


    for (int i = batchSize; i < numPixels; i++) {
        uint16_t alpha = pixel[3];
        pixel[0] = (pixel[0] * alpha + 32768) >> 16;
        pixel[1] = (pixel[1] * alpha + 32768) >> 16;
        pixel[2] = (pixel[2] * alpha + 32768) >> 16;
        pixel += 4;
    }
}

#endif

void premultiply_alpha64(uint16_t *rgba_data, int num_pixels) {
    uint16_t *pixel = rgba_data;
    for (int i = 0; i < num_pixels; ++i) {
        uint16_t alpha = pixel[3];
        pixel[0] = (pixel[0] * alpha + 32768) >> 16;
        pixel[1] = (pixel[1] * alpha + 32768) >> 16;
        pixel[2] = (pixel[2] * alpha + 32768) >> 16;
        pixel += 4;
    }
}
