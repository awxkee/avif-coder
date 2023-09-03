//
// Created by Radzivon Bartoshyk on 02/09/2023.
//

#ifndef AVIF_RGBA_TO_BGRA_H
#define AVIF_RGBA_TO_BGRA_H

#include <cstdint>

void convertRGBAtoBGRA(const uint8_t* rgba, uint8_t* bgra, int numPixels);
void convertRGBA64toBGRA64(const uint16_t * rgba, uint16_t* bgra, int numPixels);

#if HAVE_NEON
void convertRGBA64toBGRA64_NEON(const uint16_t* rgba, uint16_t* bgra, int numPixels);
void convertRGBAtoBGRA_NEON(const uint8_t* rgba, uint8_t* abgr, int numPixels);
#endif

#endif //AVIF_RGBA_TO_BGRA_H
