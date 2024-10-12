//
// Created by Radzivon Bartoshyk on 11/10/2024.
//

#include "Rec2408ToneMapper.h"

void Rec2408ToneMapper::transferTone(float *inPlace, uint32_t width) {
    float *targetPlace = inPlace;

    const float primaryR = this->lumaPrimaries[0];
    const float primaryG = this->lumaPrimaries[1];
    const float primaryB = this->lumaPrimaries[2];

    const float vWeightA = this->weightA;
    const float vWeightB = this->weightB;

    for (uint32_t x = 0; x < width; ++x) {
        float r = targetPlace[0];
        float g = targetPlace[1];
        float b = targetPlace[2];
        float Lin =
                r * primaryR + g * primaryG + b * primaryB;
        if (Lin == 0) {
            continue;
        }
        float shScale = (1.f + vWeightA * Lin) / (1.f + vWeightB * Lin);
        r = r * shScale;
        g = g * shScale;
        b = b * shScale;
        targetPlace[0] = r;
        targetPlace[1] = g;
        targetPlace[2] = b;
        targetPlace += 3;
    }
}