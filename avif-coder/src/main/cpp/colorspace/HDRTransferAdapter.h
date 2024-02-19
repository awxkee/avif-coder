/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 07/09/2023
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


#ifndef AVIF_HDRTRANSFERADAPTER_H
#define AVIF_HDRTRANSFERADAPTER_H

#include <cstdint>
#include "ColorSpaceProfile.h"
#include "Eigen/Eigen"

enum GammaCurve {
    Rec2020, DCIP3, GAMMA, Rec709, sRGB, NONE
};

enum GamutTransferFunction {
    SKIP, PQ, HLG, SMPTE428, EOTF_GAMMA, EOTF_BT601, EOTF_BT709, EOTF_SMPTE240,
    EOTF_LOG100, EOTF_LOG100SRT10, EOTF_IEC_61966, EOTF_BT1361
};

enum CurveToneMapper {
    REC2408 = 1, LOGARITHMIC = 2
};

class HDRTransferAdapter {
public:
    HDRTransferAdapter(uint8_t *rgbaData, int stride, int width, int height,
                       bool halfFloats, int bitDepth, GammaCurve gammaCorrection,
                       GamutTransferFunction function, CurveToneMapper toneMapper,
                       Eigen::Matrix3f *conversion,
                       float gamma,
                       bool useChromaticAdaptation)
            : function(function),
              gammaCorrection(gammaCorrection),
              bitDepth(bitDepth),
              halfFloats(halfFloats),
              rgbaData(rgbaData),
              stride(stride),
              width(width),
              height(height),
              toneMapper(toneMapper),
              mColorProfileConversion(conversion),
              gamma(gamma),
              useChromaticAdaptation(useChromaticAdaptation) {
    }

    void transfer();

private:
    const bool halfFloats;
    const int stride;
    const int bitDepth;
    const int width;
    const int height;
    const GamutTransferFunction function;
    uint8_t *rgbaData;
    const GammaCurve gammaCorrection;
    const CurveToneMapper toneMapper;
    Eigen::Matrix3f *mColorProfileConversion;
    const float gamma;
    bool useChromaticAdaptation;
protected:
};


#endif //AVIF_HDRTRANSFERADAPTER_H
