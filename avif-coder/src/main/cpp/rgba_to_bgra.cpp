//
// Created by Radzivon Bartoshyk on 02/09/2023.
//
#include <cstdint>

void convertRGBAtoBGRA(const uint8_t* rgba, uint8_t* bgra, int numPixels) {
    for (int i = 0; i < numPixels; ++i) {
        bgra[0] = rgba[2]; // Swap R and B
        bgra[1] = rgba[1]; // G remains the same
        bgra[2] = rgba[0]; // Swap B and R
        bgra[3] = rgba[3]; // Alpha remains the same

        rgba += 4;
        bgra += 4;
    }
}

void convertRGBA64toBGRA64(const uint16_t * rgba, uint16_t* bgra, int numPixels) {
    for (int i = 0; i < numPixels; ++i) {
        bgra[0] = rgba[2]; // Swap R and B
        bgra[1] = rgba[1]; // G remains the same
        bgra[2] = rgba[0]; // Swap B and R
        bgra[3] = rgba[3]; // Alpha remains the same

        rgba += 4;
        bgra += 4;
    }
}