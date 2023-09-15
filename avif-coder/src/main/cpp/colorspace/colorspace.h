//
// Created by Radzivon Bartoshyk on 02/09/2023.
//

#ifndef AVIF_COLORSPACE_H
#define AVIF_COLORSPACE_H

#include <vector>
#include "Rec2020Colorspace.h"
#include "LinearExtendedRec2020.h"
#include "displayP3Colorspace.h"
#include "LinearSRGBColorspace.h"
#include "Rec709Colorspace.h"
#include "lcms2.h"

void
convertUseICC(std::shared_ptr<uint8_t> &vector, int stride, int width, int height,
              const unsigned char *colorSpace, size_t colorSpaceSize,
              bool image16Bits, int *newStride);


class ColorSpace {
public:
    ColorSpace(cmsHPROFILE profile) {
        this->cmsProfile = profile;
    };

    ~ColorSpace() {
        cmsCloseProfile(this->cmsProfile);
    }

protected:
    cmsHPROFILE cmsProfile;
};

cmsHPROFILE colorspacesCreateSrgbProfile(bool v2);

cmsHPROFILE colorspacesCreatePqP3RgbProfile();

cmsHPROFILE colorspacesCreateHlgP3RgbProfile();

cmsHPROFILE colorspacesCreateLinearProphotoRgbProfile();

cmsHPROFILE colorspacesCreateLinearInfraredProfile();

cmsHPROFILE colorspacesCreateLinearRec2020RgbProfile();

cmsHPROFILE colorspacesCreatePqRec2020RgbProfile();

cmsHPROFILE colorspacesCreateLinearRec709RgbProfile();

static ColorSpace colorspacesCreateAdobergbProfile();

static ColorSpace colorspacesCreateDisplayP3RgbProfile();

cmsHPROFILE createGammaCorrectionProfile(double gamma);
void
convertUseProfiles(std::shared_ptr<uint8_t> &vector, int stride,
                   cmsHPROFILE srcProfile,
                   int width, int height,
                   cmsHPROFILE dstProfile,
                   bool image16Bits, int *newStride);

#endif //AVIF_COLORSPACE_H
