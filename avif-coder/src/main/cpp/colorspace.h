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

void convertUseDefinedColorSpace(std::shared_ptr<char> &vector, int stride, int height,
                                 const unsigned char *colorSpace, size_t colorSpaceSize);

#endif //AVIF_COLORSPACE_H
