//
// Created by Radzivon Bartoshyk on 11/10/2024.
//

#include "Trc.h"
#include <cmath>
#include <algorithm>

float avifToLinear709(float gamma) {
    if (gamma < 0.0f) {
        return 0.0f;
    } else if (gamma < 4.5f * 0.018053968510807f) {
        return gamma / 4.5f;
    } else if (gamma < 1.0f) {
        return powf((gamma + 0.09929682680944f) / 1.09929682680944f, 1.0f / 0.45f);
    } else {
        return 1.0f;
    }
}

float avifToGamma709(float linear) {
    if (linear < 0.0f) {
        return 0.0f;
    } else if (linear < 0.018053968510807f) {
        return linear * 4.5f;
    } else if (linear < 1.0f) {
        return 1.09929682680944f * powf(linear, 0.45f) - 0.09929682680944f;
    } else {
        return 1.0f;
    }
}

float avifToLinear470M(float gamma) {
    return powf(std::clamp(gamma, 0.0f, 1.0f), 2.2f);
}

float avifToGamma470M(float linear) {
    return powf(std::clamp(linear, 0.0f, 1.0f), 1.0f / 2.2f);
}

float avifToLinear470BG(float gamma) {
    return powf(std::clamp(gamma, 0.0f, 1.0f), 2.8f);
}

float avifToGamma470BG(float linear) {
    return powf(std::clamp(linear, 0.0f, 1.0f), 1.0f / 2.8f);
}

float avifToLinearSMPTE240(float gamma) {
    if (gamma < 0.0f) {
        return 0.0f;
    } else if (gamma < 4.0f * 0.022821585529445f) {
        return gamma / 4.0f;
    } else if (gamma < 1.0f) {
        return powf((gamma + 0.111572195921731f) / 1.111572195921731f, 1.0f / 0.45f);
    } else {
        return 1.0f;
    }
}

float avifToGammaSMPTE240(float linear) {
    if (linear < 0.0f) {
        return 0.0f;
    } else if (linear < 0.022821585529445f) {
        return linear * 4.0f;
    } else if (linear < 1.0f) {
        return 1.111572195921731f * powf(linear, 0.45f) - 0.111572195921731f;
    } else {
        return 1.0f;
    }
}

float avifToGammaLinear(float gamma) {
    return std::clamp(gamma, 0.0f, 1.0f);
}

float avifToLinearLog100(float gamma) {
    // The function is non-bijective so choose the middle of [0, 0.01].
    const float mid_interval = 0.01f / 2.f;
    return (gamma <= 0.0f) ? mid_interval : powf(10.0f, 2.f * (std::min(gamma, 1.f) - 1.0f));
}

float avifToGammaLog100(float linear) {
    return linear <= 0.01f ? 0.0f : 1.0f + log10f(std::min(linear, 1.0f)) / 2.0f;
}

float avifToLinearLog100Sqrt10(float gamma) {
    // The function is non-bijective so choose the middle of [0, 0.00316227766f].
    const float mid_interval = 0.00316227766f / 2.f;
    return (gamma <= 0.0f) ? mid_interval : powf(10.0f, 2.5f * (std::min(gamma, 1.f) - 1.0f));
}

float avifToGammaLog100Sqrt10(float linear) {
    return linear <= 0.00316227766f ? 0.0f : 1.0f + log10f(std::min(linear, 1.0f)) / 2.5f;
}

float avifToLinearIEC61966(float gamma) {
    if (gamma < -4.5f * 0.018053968510807f) {
        return powf((-gamma + 0.09929682680944f) / -1.09929682680944f, 1.0f / 0.45f);
    } else if (gamma < 4.5f * 0.018053968510807f) {
        return gamma / 4.5f;
    } else {
        return powf((gamma + 0.09929682680944f) / 1.09929682680944f, 1.0f / 0.45f);
    }
}

float avifToGammaIEC61966(float linear) {
    if (linear < -0.018053968510807f) {
        return -1.09929682680944f * powf(-linear, 0.45f) + 0.09929682680944f;
    } else if (linear < 0.018053968510807f) {
        return linear * 4.5f;
    } else {
        return 1.09929682680944f * powf(linear, 0.45f) - 0.09929682680944f;
    }
}

float avifToLinearBT1361(float gamma) {
    if (gamma < -0.25f) {
        return -0.25f;
    } else if (gamma < 0.0f) {
        return powf((gamma - 0.02482420670236f) / -0.27482420670236f, 1.0f / 0.45f) / -4.0f;
    } else if (gamma < 4.5f * 0.018053968510807f) {
        return gamma / 4.5f;
    } else if (gamma < 1.0f) {
        return powf((gamma + 0.09929682680944f) / 1.09929682680944f, 1.0f / 0.45f);
    } else {
        return 1.0f;
    }
}

float avifToGammaBT1361(float linear) {
    if (linear < -0.25f) {
        return -0.25f;
    } else if (linear < 0.0f) {
        return -0.27482420670236f * powf(-4.0f * linear, 0.45f) + 0.02482420670236f;
    } else if (linear < 0.018053968510807f) {
        return linear * 4.5f;
    } else if (linear < 1.0f) {
        return 1.09929682680944f * powf(linear, 0.45f) - 0.09929682680944f;
    } else {
        return 1.0f;
    }
}

float avifToLinearSRGB(float gamma) {
    if (gamma < 0.0f) {
        return 0.0f;
    } else if (gamma < 12.92f * 0.0030412825601275209f) {
        return gamma / 12.92f;
    } else if (gamma < 1.0f) {
        return powf((gamma + 0.0550107189475866f) / 1.0550107189475866f, 2.4f);
    } else {
        return 1.0f;
    }
}

float avifToGammaSRGB(float linear) {
    if (linear < 0.0f) {
        return 0.0f;
    } else if (linear < 0.0030412825601275209f) {
        return linear * 12.92f;
    } else if (linear < 1.0f) {
        return 1.0550107189475866f * powf(linear, 1.0f / 2.4f) - 0.0550107189475866f;
    } else {
        return 1.0f;
    }
}

#define PQ_MAX_NITS 10000.0f
#define HLG_PEAK_LUMINANCE_NITS 1000.0f
#define SDR_WHITE_NITS 203.0f

float avifToLinearPQ(float gamma) {
    if (gamma > 0.0f) {
        const float powGamma = powf(gamma, 1.0f / 78.84375f);
        const float num = std::max(powGamma - 0.8359375f, 0.0f);
        const float den = std::max(18.8515625f - 18.6875f * powGamma, FLT_MIN);
        const float linear = powf(num / den, 1.0f / 0.1593017578125f);
        // Scale so that SDR white is 1.0 (extended SDR).
        return linear * PQ_MAX_NITS / SDR_WHITE_NITS;
    } else {
        return 0.0f;
    }
}

float avifToGammaPQ(float linear) {
    if (linear > 0.0f) {
        // Scale from extended SDR range to [0.0, 1.0].
        linear = std::clamp(linear * SDR_WHITE_NITS / PQ_MAX_NITS, 0.0f, 1.0f);
        const float powLinear = powf(linear, 0.1593017578125f);
        const float num = 0.1640625f * powLinear - 0.1640625f;
        const float den = 1.0f + 18.6875f * powLinear;
        return powf(1.0f + num / den, 78.84375f);
    } else {
        return 0.0f;
    }
}

float avifToLinearSMPTE428(float gamma) {
    return powf(std::max(gamma, 0.0f), 2.6f) / 0.91655527974030934f;
}

float avifToGammaSMPTE428(float linear) {
    return powf(0.91655527974030934f * std::max(linear, 0.0f), 1.0f / 2.6f);
}

// Formula from ITU-R BT.2100-2
// Assumes Lw=1000 (max display luminance in nits).
// For simplicity, approximates Ys (which should be 0.2627*r+0.6780*g+0.0593*b)
// to the input value (r, g, or b depending on the current channel).
float avifToLinearHLG(float gamma) {
    // Inverse OETF followed by the OOTF, see Table 5 in ITU-R BT.2100-2 page 7.
    // Note that this differs slightly from  ITU-T H.273 which doesn't use the OOTF.
    if (gamma < 0.0f) {
        return 0.0f;
    }
    float linear = 0.0f;
    if (gamma <= 0.5f) {
        linear = powf((gamma * gamma) * (1.0f / 3.0f), 1.2f);
    } else {
        linear = powf((expf((gamma - 0.55991073f) / 0.17883277f) + 0.28466892f) / 12.0f, 1.2f);
    }
    // Scale so that SDR white is 1.0 (extended SDR).
    return linear * HLG_PEAK_LUMINANCE_NITS / SDR_WHITE_NITS;
}

float avifToGammaHLG(float linear) {
    // Scale from extended SDR range to [0.0, 1.0].
    linear = std::clamp(linear * SDR_WHITE_NITS / HLG_PEAK_LUMINANCE_NITS, 0.0f, 1.0f);
    // Inverse OOTF followed by OETF see Table 5 and Note 5i in ITU-R BT.2100-2 page 7-8.
    linear = powf(linear, 1.0f / 1.2f);
    if (linear < 0.0f) {
        return 0.0f;
    } else if (linear <= (1.0f / 12.0f)) {
        return sqrtf(3.0f * linear);
    } else {
        return 0.17883277f * logf(12.0f * linear - 0.28466892f) + 0.55991073f;
    }
}

float toLinear(float gamma, TransferFunction trc) {
    switch (trc) {
        case Srgb:
            return avifToLinearSRGB(gamma);
        case Itur709:
            return avifToLinear709(gamma);
        case Gamma2p2:
            return avifToLinear470M(gamma);
        case Gamma2p8:
            return avifToLinear470BG(gamma);
        case Smpte428:
            return avifToLinearSMPTE428(gamma);
        case Log100:
            return avifToLinearLog100(gamma);
        case Log100Sqrt10:
            return avifToLinearLog100Sqrt10(gamma);
        case Bt1361:
            return avifToLinearBT1361(gamma);
        case Smpte240:
            return avifToLinearSMPTE240(gamma);
        case Pq:
            return avifToLinearPQ(gamma);
        case Hlg:
            return avifToLinearHLG(gamma);
        case Linear:
            return std::clamp(gamma, 0.0f, 1.0f);
        case Iec61966:
            return avifToLinearIEC61966(gamma);
        default:
            return std::clamp(gamma, 0.0f, 1.0f);
    }
}

float toGamma(float gamma, TransferFunction trc) {
    switch (trc) {
        case Srgb:
            return avifToGammaSRGB(gamma);
        case Itur709:
            return avifToGamma709(gamma);
        case Gamma2p2:
            return avifToGamma470M(gamma);
        case Gamma2p8:
            return avifToGamma470BG(gamma);
        case Smpte428:
            return avifToGammaSMPTE428(gamma);
        case Log100:
            return avifToGammaLog100(gamma);
        case Log100Sqrt10:
            return avifToGammaLog100Sqrt10(gamma);
        case Bt1361:
            return avifToGammaBT1361(gamma);
        case Smpte240:
            return avifToGammaSMPTE240(gamma);
        case Pq:
            return avifToGammaPQ(gamma);
        case Hlg:
            return avifToGammaHLG(gamma);
        case Linear:
            return avifToGammaLinear(gamma);
        case Iec61966:
            return avifToGammaIEC61966(gamma);
        default:
            return avifToGammaLinear(gamma);
    }
}