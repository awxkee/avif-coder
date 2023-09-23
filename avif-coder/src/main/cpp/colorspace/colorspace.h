/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 02/09/2023
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

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
