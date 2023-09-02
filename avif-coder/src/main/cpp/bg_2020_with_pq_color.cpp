//
// Created by Radzivon Bartoshyk on 02/09/2023.
//

#include "bg_2020_with_pq_color.h"
#include <cmath>
#include "lcms2.h"
#include <vector>

#if HAVE_NEON

#include <arm_neon.h>

#endif

// Perceptual quantizer reverse EOTF (Electrical optical transform function)

static double maxNits = 10000.0;  // Maximum luminance in nits for BT.2020 PQ
double m1 = 0.1593017578125;  // 2610 / (4096 * 4)
double m2 = 78.84375;  // 2523 * 4096 / 4096
double c1 = 0.8359375;  // 3424 / 4096
double c2 = 18.8515625;  // 2413 * 32 / 4096
double c3 = 18.68;  // 2392 / 4096

double calculateLuminance(float red, float green, float blue) {
    return 0.299 * red + 0.587 * green + 0.114 * blue;
}

double PQInverseEOTF(double q) {
    double vv = pow(q, m1);
    double v = c1 + c2 * vv;
    double k = 1 + c3 * vv;
    double rs = v / k;
    return pow(rs, m2);
}

void convertBP2020PQToRGBA(std::shared_ptr<char> &vector, int stride, int height) {
    auto pixel = vector.get();

    for (int i = 0; i < stride / 4 * height; i++) {
        float r = float(pixel[0]) / 255.0f;
        float g = float(pixel[1]) / 255.0f;
        float b = float(pixel[2]) / 255.0f;

        // Convert from BT.2020 PQ to linear light
        double linearR = pow(r, maxNits / 4096.0);
        double linearG = pow(g, maxNits / 4096.0);
        double linearB = pow(b, maxNits / 4096.0);

        // Convert from linear light to sRGB
        double sR = 3.2406 * linearR - 1.5372 * linearG - 0.4986 * linearB;
        double sG = -0.9689 * linearR + 1.8758 * linearG + 0.0415 * linearB;
        double sB = 0.0557 * linearR - 0.2040 * linearG + 1.0570 * linearB;

        // Apply gamma correction for sRGB
        if (sR <= 0.0031308)
            sR = 12.92 * sR;
        else
            sR = 1.055 * pow(sR, 1.0 / 2.4) - 0.055;

        if (sG <= 0.0031308)
            sG = 12.92 * sG;
        else
            sG = 1.055 * pow(sG, 1.0 / 2.4) - 0.055;

        if (sB <= 0.0031308)
            sB = 12.92 * sB;
        else
            sB = 1.055 * pow(sB, 1.0 / 2.4) - 0.055;

//         Clamp the values to the [0, 1] range
//        sR = std::max(0.0, std::min(1.0, sR));
//        sG = std::max(0.0, std::min(1.0, sG));
//        sB = std::max(0.0, std::min(1.0, sB));

//         Scale the values to the [0, 255] range
        int sRGB_R = std::max(std::min(static_cast<int>(lround(sR * 255.0 + 0.5)), 255), 0);
        int sRGB_G = std::max(std::min(static_cast<int>(lround(sG * 255.0 + 0.5)), 255), 0);
        int sRGB_B = std::max(std::min(static_cast<int>(lround(sB * 255.0 + 0.5)), 255), 0);
        pixel[0] = sRGB_R;
        pixel[1] = sRGB_G;
        pixel[2] = sRGB_B;

        pixel += 4;
    }
}
