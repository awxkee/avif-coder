//
// Created by Radzivon Bartoshyk on 11/10/2024.
//

#include "LogarithmicToneMapper.h"

void LogarithmicToneMapper::transferTone(float *inPlace, uint32_t width) {
    float *targetPlace = inPlace;

    const float primaryR = this->lumaPrimaries[0];
    const float primaryG = this->lumaPrimaries[1];
    const float primaryB = this->lumaPrimaries[2];

    const float vDen = this->den;

    for (uint32_t x = 0; x < width; ++x) {
        float r = targetPlace[0];
        float g = targetPlace[1];
        float b = targetPlace[2];
        float Lin =
                r * primaryR + g * primaryG + b * primaryB;
        if (Lin == 0) {
            continue;
        }
        float Lout = std::logf(std::abs(1 + Lin)) * vDen;
        float shScale = Lout / Lin;
        r = r * shScale;
        g = g * shScale;
        b = b * shScale;
        targetPlace[0] = r;
        targetPlace[1] = g;
        targetPlace[2] = b;
        targetPlace += 3;
    }
}