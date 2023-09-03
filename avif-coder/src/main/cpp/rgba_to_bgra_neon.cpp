//
// Created by Radzivon Bartoshyk on 02/09/2023.
//

#if HAVE_NEON
#include <arm_neon.h>

void convertRGBA64toBGRA64_NEON(const uint16_t* rgba, uint16_t* bgra, int numPixels) {
    int numIterations = numPixels / 4;
    int remainingPixels = numPixels % 4;

    uint8x16_t swapMask = vsetq_lane_u8(2, vdupq_n_u8(0), 0);
    swapMask = vsetq_lane_u8(0, swapMask, 2);
    for (int i = 0; i < numIterations; ++i) {
        auto pixels = vld4q_u16((const uint16_t*)rgba);

        uint16x8x4_t bgraPixels;
        bgraPixels.val[0] = vqtbl1q_u8(pixels.val[2], swapMask);
        bgraPixels.val[1] = pixels.val[1];
        bgraPixels.val[2] = vqtbl1q_u8(pixels.val[0], swapMask);
        bgraPixels.val[3] = pixels.val[3];

        // Store the resulting BGRA pixels
        vst4q_u16((uint16_t*)bgra, bgraPixels);

        rgba += 4 * 4;
        bgra += 4 * 4;
    }

    // Handle any remaining pixels (less than 4)
    for (int i = 0; i < remainingPixels; ++i) {
        // Swap R and B channels
        bgra[i] = rgba[2];  // B
        bgra[i + 1] = rgba[1];  // G
        bgra[i + 2] = rgba[0];  // R
        bgra[i + 3] = rgba[3];  // A

        // Move to the next pixel
        rgba += 4;
        bgra += 4;
    }
}

void convertRGBAtoBGRA_NEON(const uint8_t *rgba, uint8_t *bgra, int numPixels) {
    int numIterations = numPixels / 4;
    int remainingPixels = numPixels % 4;

    uint8x16_t swapMask = vsetq_lane_u8(2, vdupq_n_u8(0), 0);
    swapMask = vsetq_lane_u8(0, swapMask, 2);
    for (int i = 0; i < numIterations; ++i) {
        uint8x16x4_t pixels = vld4q_u8(rgba);

        uint8x16x4_t bgraPixels;
        bgraPixels.val[0] = vqtbl1q_u8(pixels.val[2], swapMask);
        bgraPixels.val[1] = pixels.val[1];
        bgraPixels.val[2] = vqtbl1q_u8(pixels.val[0], swapMask);
        bgraPixels.val[3] = pixels.val[3];

        // Store the resulting BGRA pixels
        vst4q_u8((uint8_t *)bgra, bgraPixels);

        rgba += 4 * 4;
        bgra += 4 * 4;
    }

    // Handle any remaining pixels (less than 4)
    for (int i = 0; i < remainingPixels; ++i) {
        // Swap R and B channels
        bgra[i] = rgba[2];  // B
        bgra[i + 1] = rgba[1];  // G
        bgra[i + 2] = rgba[0];  // R
        bgra[i + 3] = rgba[3];  // A

        // Move to the next pixel
        rgba += 4;
        bgra += 4;
    }
}
#endif