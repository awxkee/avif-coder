//
// Created by Radzivon Bartoshyk on 11/10/2024.
//

#ifndef AVIF_REC2408TONEMAPPER_H
#define AVIF_REC2408TONEMAPPER_H

#include <vector>

class Rec2408ToneMapper {
public:
    Rec2408ToneMapper(const float contentMaxBrightness,
                      const float displayMaxBrightness,
                      const float whitePoint,
                      const float primaries[3]) {
        std::copy(primaries, primaries + 3, lumaPrimaries);

        this->Ld = contentMaxBrightness / whitePoint;
        this->weightA = (displayMaxBrightness / whitePoint) / (Ld * Ld);
        this->weightB = 1.0f / (displayMaxBrightness / whitePoint);
    }

    void transferTone(float *inPlace, uint32_t width);

private:
    float lumaPrimaries[3] = {0};
    float Ld = 0;
    float weightA = 0;
    float weightB = 0;
};

#endif //AVIF_REC2408TONEMAPPER_H
