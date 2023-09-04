//
// Created by Radzivon Bartoshyk on 02/09/2023.
//

#ifndef AVIF_COLORSPACE_H
#define AVIF_COLORSPACE_H

#include <vector>
#include "bt2020_pq_colorspace.h"
#include "bt2020_colorspace.h"
#include "linear_extended_bt2020.h"
#include "displayP3_colorspace.h"
#include "linear_srgb_colorspace.h"
#include "bt709_colorspace.h"
#include "displayP3_HLG.h"
#include "itur2100_pq_full.h"

void convertUseDefinedColorSpace(std::shared_ptr<char> &vector, int stride, int width, int height,
                                 const unsigned char *colorSpace, size_t colorSpaceSize,
                                 bool image16Bits);

#endif //AVIF_COLORSPACE_H
