//
// Created by Radzivon Bartoshyk on 02/09/2023.
//

#ifndef AVIF_COLORSPACE_H
#define AVIF_COLORSPACE_H

#include <vector>
#include "bt2020_colorspace.h"
#include "linear_extended_bt2020.h"
#include "displayP3_colorspace.h"
#include "linear_srgb_colorspace.h"
#include "bt709_colorspace.h"
#include "displayP3_HLG.h"
#include "lcms2.h"

void
convertUseICC(std::shared_ptr<uint8_t> &vector, int stride, int width, int height,
              const unsigned char *colorSpace, size_t colorSpaceSize,
              bool image16Bits, int *newStride);

cmsHPROFILE colorspacesCreateSrgbProfile(bool v2);

cmsHPROFILE colorspacesCreatePqP3RgbProfile();

cmsHPROFILE colorspacesCreateHlgP3RgbProfile();

cmsHPROFILE colorspacesCreateLinearProphotoRgbProfile();

cmsHPROFILE colorspacesCreateLinearInfraredProfile();

cmsHPROFILE colorspacesCreateLinearRec2020RgbProfile();

cmsHPROFILE colorspacesCreatePqRec2020RgbProfile();

cmsHPROFILE colorspacesCreateLinearRec709RgbProfile();

cmsHPROFILE createGammaCorrectionProfile(double gamma);

void
convertUseProfiles(std::shared_ptr<uint8_t> &vector, int stride,
                   cmsHPROFILE srcProfile,
                   int width, int height,
                   cmsHPROFILE dstProfile,
                   bool image16Bits, int *newStride);

#endif //AVIF_COLORSPACE_H
