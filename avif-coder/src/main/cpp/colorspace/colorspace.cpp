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

#include "colorspace/colorspace.h"
#include <cmath>
#include "lcms2.h"
#include <vector>
#include <thread>
#include <android/log.h>
#include "concurrency.hpp"

using namespace std;

static const cmsCIEXYZ d65 = {0.95045471, 1.00000000, 1.08905029};

//D65 (sRGB, AdobeRGB, Rec2020)
static const cmsCIExyY D65xyY = {0.312700492, 0.329000939, 1.0};

//D60
//static const cmsCIExyY d60 = {0.32168, 0.33767, 1.0};

//D50 (ProPhoto RGB)
static const cmsCIExyY D50xyY = {0.3457, 0.3585, 1.0};

// D65:
static const cmsCIExyYTRIPLE sRGB_Primaries = {
    {0.6400, 0.3300, 1.0}, // red
    {0.3000, 0.6000, 1.0}, // green
    {0.1500, 0.0600, 1.0}  // blue
};

// D65:
static const cmsCIExyYTRIPLE Rec2020_Primaries = {
    {0.7080, 0.2920, 1.0}, // red
    {0.1700, 0.7970, 1.0}, // green
    {0.1310, 0.0460, 1.0}  // blue
};

// D65:
static const cmsCIExyYTRIPLE Rec709_Primaries = {
    {0.6400, 0.3300, 1.0}, // red
    {0.3000, 0.6000, 1.0}, // green
    {0.1500, 0.0600, 1.0}  // blue
};

// D65:
static const cmsCIExyYTRIPLE Adobe_Primaries = {
    {0.6400, 0.3300, 1.0}, // red
    {0.2100, 0.7100, 1.0}, // green
    {0.1500, 0.0600, 1.0}  // blue
};

// D65:
static const cmsCIExyYTRIPLE P3_Primaries = {
    {0.680, 0.320, 1.0}, // red
    {0.265, 0.690, 1.0}, // green
    {0.150, 0.060, 1.0}  // blue
};

// https://en.wikipedia.org/wiki/ProPhoto_RGB_color_space
// D50:
static const cmsCIExyYTRIPLE ProPhoto_Primaries = {
    /*       x,        y,       Y */
    {0.734699, 0.265301, 1.0000}, /* red   */
    {0.159597, 0.840403, 1.0000}, /* green */
    {0.036598, 0.000105, 1.0000}, /* blue  */
};

static cmsToneCurve *colorSpacesCreateTransfer(int32_t size, double (*fct)(double)) {
  float *values = (float *) malloc(size * sizeof(float));

  for (int32_t i = 0; i < size; ++i) {
    const double x = (float) i / (float) (size - 1);
    double y = std::min(fct(x), 1.0);
    if (y > 1.0) y = 1.0;
    values[i] = (float) y;
  }

  cmsToneCurve *result = cmsBuildTabulatedToneCurveFloat(nullptr, size, values);
  free(values);
  return result;
}

static cmsHPROFILE createLcmsProfile(const char *desc, const char *dmdd,
                                     const cmsCIExyY *whitepoint,
                                     const cmsCIExyYTRIPLE *primaries,
                                     cmsToneCurve *trc,
                                     bool v2) {
  cmsMLU *mlu1 = cmsMLUalloc(nullptr, 1);
  cmsMLU *mlu2 = cmsMLUalloc(nullptr, 1);
  cmsMLU *mlu3 = cmsMLUalloc(nullptr, 1);
  cmsMLU *mlu4 = cmsMLUalloc(nullptr, 1);

  cmsToneCurve *out_curves[3] = {trc, trc, trc};
  cmsHPROFILE profile = cmsCreateRGBProfile(whitepoint, primaries, out_curves);

  if (v2) {
    cmsSetProfileVersion(profile, 2.1);
    const cmsCIEXYZ black = {0, 0, 0};
    cmsWriteTag(profile, cmsSigMediaBlackPointTag, &black);
    cmsWriteTag(profile, cmsSigMediaWhitePointTag, whitepoint);
    cmsSetDeviceClass(profile, cmsSigDisplayClass);
  }

  cmsSetHeaderFlags(profile, cmsEmbeddedProfileTrue | cmsUseAnywhere);

  cmsMLUsetASCII(mlu1, "en", "US", "Public Domain");
  cmsWriteTag(profile, cmsSigCopyrightTag, mlu1);

  cmsMLUsetASCII(mlu2, "en", "US", desc);
  cmsWriteTag(profile, cmsSigProfileDescriptionTag, mlu2);

  cmsMLUsetASCII(mlu3, "en", "US", dmdd);
  cmsWriteTag(profile, cmsSigDeviceModelDescTag, mlu3);

  cmsMLUsetASCII(mlu4, "en", "US", "AVIF Coder");
  cmsWriteTag(profile, cmsSigDeviceMfgDescTag, mlu4);

  cmsMLUfree(mlu1);
  cmsMLUfree(mlu2);
  cmsMLUfree(mlu3);
  cmsMLUfree(mlu4);

  return profile;
}

// Function to create a gamma-corrected tone curve
cmsToneCurve *createGammaToneCurve(double gamma) {
  // Define the number of entries in the table (e.g., 1024)
  int numEntries = 1024;

  // Allocate memory for the curve data
  std::vector<float> values(numEntries);

  // Calculate the gamma-corrected values and store them in the table
  for (int i = 0; i < numEntries; ++i) {
    double input = static_cast<double>(i) / (numEntries - 1);
    double output = std::powf(input, 1.0 / gamma);
    values[i] = static_cast<float>(output);
  }

  // Create the tabulated tone curve
  return cmsBuildTabulatedToneCurveFloat(nullptr, numEntries, values.data());
}

// Function to create an LCMS profile with gamma correction
cmsHPROFILE createGammaCorrectionProfile(double gamma) {
  cmsToneCurve *transferFunction = cmsBuildGamma(nullptr, gamma);

  cmsHPROFILE profile = createLcmsProfile("RGB", "RGB",
                                          &D65xyY, &sRGB_Primaries, transferFunction, TRUE);

  cmsFreeToneCurve(transferFunction);

  return profile;
}

cmsHPROFILE colorspacesCreateSrgbProfile(bool v2) {
  cmsFloat64Number srgb_parameters[5] = {2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92, 0.04045};
  cmsToneCurve *transferFunction = cmsBuildParametricToneCurve(nullptr, 4, srgb_parameters);

  cmsHPROFILE profile = createLcmsProfile("sRGB", "sRGB",
                                          &D65xyY, &sRGB_Primaries, transferFunction, v2);

  cmsFreeToneCurve(transferFunction);

  return profile;
}

static cmsHPROFILE colorspacesCreateXyzProfile() {
  cmsHPROFILE hXYZ = cmsCreateXYZProfile();
  cmsSetPCS(hXYZ, cmsSigXYZData);
  cmsSetHeaderRenderingIntent(hXYZ, INTENT_PERCEPTUAL);

  if (hXYZ == nullptr) return nullptr;

  cmsSetProfileVersion(hXYZ, 2.1);
  cmsMLU *mlu0 = cmsMLUalloc(nullptr, 1);
  cmsMLUsetASCII(mlu0, "en", "US", "(dt internal)");
  cmsMLU *mlu1 = cmsMLUalloc(nullptr, 1);
  cmsMLUsetASCII(mlu1, "en", "US", "linear XYZ");
  cmsMLU *mlu2 = cmsMLUalloc(nullptr, 1);
  cmsMLUsetASCII(mlu2, "en", "US", "AVIF linear XYZ");
  cmsWriteTag(hXYZ, cmsSigDeviceMfgDescTag, mlu0);
  cmsWriteTag(hXYZ, cmsSigDeviceModelDescTag, mlu1);
  // this will only be displayed when the embedded profile is read by for example GIMP
  cmsWriteTag(hXYZ, cmsSigProfileDescriptionTag, mlu2);
  cmsMLUfree(mlu0);
  cmsMLUfree(mlu1);
  cmsMLUfree(mlu2);

  return hXYZ;
}

ColorSpace colorspacesCreateDisplayP3RgbProfile() {
  cmsFloat64Number srgb_parameters[5] =
      {2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92, 0.04045};
  cmsToneCurve *transferFunction = cmsBuildParametricToneCurve(nullptr, 4, srgb_parameters);

  cmsHPROFILE profile = createLcmsProfile("Display P3 RGB", "Display P3 RGB",
                                          &D65xyY, &P3_Primaries,
                                          transferFunction, TRUE);

  cmsFreeToneCurve(transferFunction);

  return {profile};
}

ColorSpace colorspacesCreateDCIP3RgbProfile() {
  //DCI P3 gamma 2.6
  cmsToneCurve *transferFunction = cmsBuildGamma(nullptr, 2.6f);

  cmsHPROFILE profile = createLcmsProfile("DCI P3 RGB", "DCI P3 RGB",
                                          &D65xyY, &P3_Primaries,
                                          transferFunction, TRUE);

  cmsFreeToneCurve(transferFunction);

  return {profile};
}

ColorSpace colorspacesCreateAdobergbProfile() {
  // AdobeRGB's "2.2" gamma is technically defined as 2 + 51/256
  cmsToneCurve *transferFunction = cmsBuildGamma(nullptr, 2.19921875);

  cmsHPROFILE profile = createLcmsProfile("Adobe RGB (compatible)", "Adobe RGB",
                                          &D65xyY, &Adobe_Primaries,
                                          transferFunction, TRUE);

  cmsFreeToneCurve(transferFunction);

  return {profile};
}

cmsHPROFILE colorspacesCreateLinearRec709RgbProfile() {
  cmsToneCurve *transferFunction = cmsBuildGamma(nullptr, 1.0);

  cmsHPROFILE profile = createLcmsProfile("Linear Rec709 RGB", "Linear Rec709 RGB",
                                          &D65xyY, &Rec709_Primaries, transferFunction, TRUE);

  cmsFreeToneCurve(transferFunction);

  return profile;
}

cmsHPROFILE colorspacesCreateLinearRec2020RgbProfile() {
  cmsToneCurve *transferFunction = cmsBuildGamma(nullptr, 1.0);

  cmsHPROFILE profile = createLcmsProfile("Linear Rec2020 RGB", "Linear Rec2020 RGB",
                                          &D65xyY, &Rec2020_Primaries, transferFunction, TRUE);

  cmsFreeToneCurve(transferFunction);

  return profile;
}

cmsHPROFILE colorspacesCreateLinearProphotoRgbProfile() {
  cmsToneCurve *transferFunction = cmsBuildGamma(nullptr, 1.0);

  cmsHPROFILE profile = createLcmsProfile("Linear ProPhoto RGB", "Linear ProPhoto RGB",
                                          &D50xyY, &ProPhoto_Primaries, transferFunction, TRUE);

  cmsFreeToneCurve(transferFunction);

  return profile;
}

cmsHPROFILE colorspacesCreateLinearInfraredProfile() {
  cmsToneCurve *transferFunction = cmsBuildGamma(nullptr, 1.0);

  // linear rgb with r and b swapped:
  cmsCIExyYTRIPLE BGR_Primaries = {sRGB_Primaries.Blue, sRGB_Primaries.Green, sRGB_Primaries.Red};

  cmsHPROFILE profile = createLcmsProfile("Linear Infrared BGR", "Darktable Linear Infrared BGR",
                                          &D65xyY, &BGR_Primaries, transferFunction, FALSE);

  cmsFreeToneCurve(transferFunction);

  return profile;
}

void
convertUseProfiles(aligned_uint8_vector &srcVector, uint32_t stride,
                   cmsHPROFILE srcProfile,
                   uint32_t width, uint32_t height,
                   cmsHPROFILE dstProfile,
                   bool image16Bits) {
  cmsContext context = cmsCreateContext(nullptr, nullptr);
  shared_ptr<void> contextPtr(context, [](void *profile) {
    cmsDeleteContext(reinterpret_cast<cmsContext>(profile));
  });
  cmsHTRANSFORM transform = cmsCreateTransform(srcProfile,
                                               image16Bits ? TYPE_RGBA_16_PREMUL : TYPE_RGBA_8,
                                               dstProfile,
                                               image16Bits ? TYPE_RGBA_16_PREMUL : TYPE_RGBA_8,
                                               INTENT_ABSOLUTE_COLORIMETRIC,
                                               cmsFLAGS_BLACKPOINTCOMPENSATION |
                                                   cmsFLAGS_NOWHITEONWHITEFIXUP |
                                                   cmsFLAGS_COPY_ALPHA);
  if (!transform) {
    // JUST RETURN without signalling error, better proceed with invalid photo than crash
    __android_log_print(ANDROID_LOG_ERROR, "AVIFCoder", "ColorProfile Creation has hailed");
    return;
  }
  shared_ptr<void> ptrTransform(transform, [](void *transform) {
    cmsDeleteTransform(reinterpret_cast<cmsHTRANSFORM>(transform));
  });

  auto mInputBuffer = srcVector.data();
  auto mTransform = ptrTransform.get();

  concurrency::parallel_for(6, height, [&](int y) {
    cmsDoTransformLineStride(mTransform,
                             mInputBuffer + stride * y,
                             mInputBuffer + stride * y, width, 1,
                             stride, stride, 0, 0);
  });
}

void
convertUseICC(aligned_uint8_vector &vector, uint32_t stride, uint32_t width, uint32_t height,
              const unsigned char *colorSpace, size_t colorSpaceSize,
              bool image16Bits) {
  cmsContext context = cmsCreateContext(nullptr, nullptr);
  shared_ptr<void> contextPtr(context, [](void *profile) {
    cmsDeleteContext(reinterpret_cast<cmsContext>(profile));
  });
  cmsHPROFILE srcProfile = cmsOpenProfileFromMem(colorSpace, colorSpaceSize);
  if (!srcProfile) {
    // JUST RETURN without signalling error, better proceed with invalid photo than crash
    __android_log_print(ANDROID_LOG_ERROR, "JXLCoder", "ColorProfile Allocation Failed");
    return;
  }
  shared_ptr<void> ptrSrcProfile(srcProfile, [](void *profile) {
    cmsCloseProfile(reinterpret_cast<cmsHPROFILE>(profile));
  });
  cmsHPROFILE dstProfile = cmsCreate_sRGBProfileTHR(
      reinterpret_cast<cmsContext>(contextPtr.get()));
  shared_ptr<void> ptrDstProfile(dstProfile, [](void *profile) {
    cmsCloseProfile(reinterpret_cast<cmsHPROFILE>(profile));
  });
  cmsHTRANSFORM transform = cmsCreateTransform(ptrSrcProfile.get(),
                                               image16Bits ? TYPE_RGBA_16_PREMUL : TYPE_RGBA_8,
                                               ptrDstProfile.get(),
                                               image16Bits ? TYPE_RGBA_16_PREMUL : TYPE_RGBA_8,
                                               INTENT_PERCEPTUAL,
                                               cmsFLAGS_BLACKPOINTCOMPENSATION |
                                                   cmsFLAGS_NOWHITEONWHITEFIXUP |
                                                   cmsFLAGS_COPY_ALPHA);
  if (!transform) {
    // JUST RETURN without signalling error, better proceed with invalid photo than crash
    __android_log_print(ANDROID_LOG_ERROR, "AVIFCoder", "ColorProfile Creation has hailed");
    return;
  }

  convertUseProfiles(vector, stride, ptrSrcProfile.get(), width, height, ptrDstProfile.get(),
                     image16Bits);
}
