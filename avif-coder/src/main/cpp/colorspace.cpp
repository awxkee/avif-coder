//
// Created by Radzivon Bartoshyk on 02/09/2023.
//

#include "colorspace.h"
#include <cmath>
#include "lcms2.h"
#include <vector>
#include "ThreadPool.hpp"
#include <android/log.h>

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

cmsCIEXYZTRIPLE Rec709_Primaries_Prequantized;

// https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2100-2-201807-I!!PDF-F.pdf
// Hybrid Log-Gamma
static double hlgFct(double x) {
    static const double Beta = 0.04;
    static const double RA = 5.591816309728916; // 1.0 / A where A = 0.17883277
    static const double B = 0.28466892; // 1.0 - 4.0 * A
    static const double C = 0.5599107295; // 0,5 –aln(4a)

    double e = std::max(x * (1.0 - Beta) + Beta, 0.0);

    if (e == 0.0) return 0.0;

    const double sign = e;
    e = fabs(e);

    double res = 0.0;

    if (e <= 0.5) {
        res = e * e / 3.0;
    } else {
        res = (exp((e - C) * RA) + B) / 12.0;
    }

    return copysign(res, sign);
}

// https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2100-2-201807-I!!PDF-F.pdf
// Perceptual Quantization / SMPTE standard ST.2084

//double pqFct(double x) {
//        static const double M1 = 2610.0 / 16384.0;
//    static const double M2 = (2523.0 / 4096.0) * 128.0;
//    static const double C1 = 3424.0 / 4096.0;
//    static const double C2 = (2413.0 / 4096.0) * 32.0;
//    static const double C3 = (2392.0 / 4096.0) * 32.0;
//    static const double MAX_PQ = 10000;
//    if (x <= 0.0) return 0.0;
//    return std::pow((std::max(x, 0.0)), 1.0 / M2) * ((C1 + M1 * std::log(1.0 + C2 * x)) / (1.0 + C3 * x));
//}
double pqFct(double x) {
    x = std::max(x, 0.0);
    static const double M1 = 2610.0 / 16384.0;
    static const double M2 = (2523.0 / 4096.0) * 128.0;
    static const double C1 = 3424.0 / 4096.0;
    static const double C2 = (2413.0 / 4096.0) * 32.0;
    static const double C3 = (2392.0 / 4096.0) * 32.0;

    if (x == 0.0) return 0.0;
    const double sign = x;
    x = fabs(x);

    const double xpo = pow(x, 1.0 / M2);
    const double num = std::max(xpo - C1, 0.0);
    const double den = C2 - C3 * xpo;
    const double res = pow(num / den, 1.0 / M1);

    return copysign(res, sign);
}

static constexpr double kA = 0.17883277;
static constexpr double kRA = 1.0 / kA;
static constexpr double kB = 1 - 4 * kA;
static constexpr double kC = 0.5599107295;
static constexpr double kDiv12 = 1.0 / 12;

static constexpr double kM1 = 2610.0 / 16384;
static constexpr double kM2 = (2523.0 / 4096) * 128;
static constexpr double kC1 = 3424.0 / 4096;
static constexpr double kC2 = (2413.0 / 4096) * 32;
static constexpr double kC3 = (2392.0 / 4096) * 32;

double DisplayFromEncoded(double e) {
    if (e == 0.0) return 0.0;
    const double original_sign = e;
    e = std::abs(e);

    const double xp = std::pow(e, 1.0 / kM2);
    const double num = std::max(xp - kC1, 0.0);
    const double den = kC2 - kC3 * xp;
    const double d = std::pow(num / den, 1.0 / kM1);
    return copysignf(d, original_sign);
}


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

    cmsMLUsetASCII(mlu4, "en", "US", "Darktable");
    cmsWriteTag(profile, cmsSigDeviceMfgDescTag, mlu4);

    cmsMLUfree(mlu1);
    cmsMLUfree(mlu2);
    cmsMLUfree(mlu3);
    cmsMLUfree(mlu4);

    return profile;
}

cmsHPROFILE colorspacesCreatePqRec2020RgbProfile() {
    cmsToneCurve *transferFunction = colorSpacesCreateTransfer(4096, pqFct);

    cmsHPROFILE profile = createLcmsProfile("PQ Rec2020 RGB", "PQ Rec2020 RGB",
                                            &D65xyY, &Rec2020_Primaries, transferFunction, TRUE);

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
        double output = std::pow(input, 1.0 / gamma);
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

static cmsHPROFILE dt_colorspaces_create_xyz_profile() {
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
    cmsMLUsetASCII(mlu2, "en", "US", "Darktable linear XYZ");
    cmsWriteTag(hXYZ, cmsSigDeviceMfgDescTag, mlu0);
    cmsWriteTag(hXYZ, cmsSigDeviceModelDescTag, mlu1);
    // this will only be displayed when the embedded profile is read by for example GIMP
    cmsWriteTag(hXYZ, cmsSigProfileDescriptionTag, mlu2);
    cmsMLUfree(mlu0);
    cmsMLUfree(mlu1);
    cmsMLUfree(mlu2);

    return hXYZ;
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

cmsHPROFILE colorspacesCreatePqP3RgbProfile() {
    cmsToneCurve *transferFunction = colorSpacesCreateTransfer(4096, pqFct);

    cmsHPROFILE profile = createLcmsProfile("PQ P3 RGB", "PQ P3 RGB",
                                            &D65xyY, &P3_Primaries, transferFunction, TRUE);

    cmsFreeToneCurve(transferFunction);

    return profile;
}

cmsHPROFILE colorspacesCreateHlgP3RgbProfile() {
    cmsToneCurve *transferFunction = colorSpacesCreateTransfer(4096, hlgFct);

    cmsHPROFILE profile = createLcmsProfile("HLG P3 RGB", "HLG P3 RGB",
                                            &D65xyY, &P3_Primaries, transferFunction, TRUE);

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
convertUseProfiles(std::shared_ptr<uint8_t> &vector, int stride,
                   cmsHPROFILE srcProfile,
                   int width, int height,
                   cmsHPROFILE dstProfile,
                   bool image16Bits, int *newStride) {
    cmsContext context = cmsCreateContext(nullptr, nullptr);
    std::shared_ptr<void> contextPtr(context, [](void *profile) {
        cmsDeleteContext(reinterpret_cast<cmsContext>(profile));
    });
    cmsHTRANSFORM transform = cmsCreateTransform(srcProfile,
                                                 image16Bits ? TYPE_RGBA_HALF_FLT : TYPE_RGBA_8,
                                                 dstProfile,
                                                 image16Bits ? TYPE_RGBA_HALF_FLT : TYPE_RGBA_8,
                                                 INTENT_ABSOLUTE_COLORIMETRIC,
                                                 cmsFLAGS_COPY_ALPHA);
    if (!transform) {
        // JUST RETURN without signalling error, better proceed with invalid photo than crash
        __android_log_print(ANDROID_LOG_ERROR, "AVIFCoder", "ColorProfile Creation has hailed");
        return;
    }
    std::shared_ptr<void> ptrTransform(transform, [](void *transform) {
        cmsDeleteTransform(reinterpret_cast<cmsHTRANSFORM>(transform));
    });

    ThreadPool pool;
    std::vector<std::future<void>> results;

    std::vector<char> iccARGB;
    int mStride = (int) (image16Bits ? sizeof(uint16_t) : sizeof(uint8_t)) * width * 4;
    int newLength = mStride * height;
    iccARGB.resize(newLength);

    for (int y = 0; y < height; ++y) {
        auto r = pool.enqueue(cmsDoTransformLineStride, ptrTransform.get(),
                              vector.get() + stride * y, iccARGB.data() + mStride * y, width, 1,
                              stride, stride, 0, 0);
        results.push_back(std::move(r));
    }

    for (auto &result: results) {
        result.wait();
    }

    std::copy(iccARGB.begin(), iccARGB.end(), vector.get());
    *newStride = mStride;
}


void
convertUseICC(std::shared_ptr<uint8_t> &vector, int stride, int width, int height,
              const unsigned char *colorSpace, size_t colorSpaceSize,
              bool image16Bits, int *newStride) {
    cmsContext context = cmsCreateContext(nullptr, nullptr);
    std::shared_ptr<void> contextPtr(context, [](void *profile) {
        cmsDeleteContext(reinterpret_cast<cmsContext>(profile));
    });
    cmsHPROFILE srcProfile = cmsOpenProfileFromMem(colorSpace, colorSpaceSize);
    if (!srcProfile) {
        // JUST RETURN without signalling error, better proceed with invalid photo than crash
        __android_log_print(ANDROID_LOG_ERROR, "JXLCoder", "ColorProfile Allocation Failed");
        return;
    }
    std::shared_ptr<void> ptrSrcProfile(srcProfile, [](void *profile) {
        cmsCloseProfile(reinterpret_cast<cmsHPROFILE>(profile));
    });
    cmsHPROFILE dstProfile = cmsCreate_sRGBProfileTHR(
            reinterpret_cast<cmsContext>(contextPtr.get()));
    std::shared_ptr<void> ptrDstProfile(dstProfile, [](void *profile) {
        cmsCloseProfile(reinterpret_cast<cmsHPROFILE>(profile));
    });
    cmsHTRANSFORM transform = cmsCreateTransform(ptrSrcProfile.get(),
                                                 image16Bits ? TYPE_RGBA_HALF_FLT : TYPE_RGBA_8,
                                                 ptrDstProfile.get(),
                                                 image16Bits ? TYPE_RGBA_HALF_FLT : TYPE_RGBA_8,
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
                       image16Bits, newStride);
}
