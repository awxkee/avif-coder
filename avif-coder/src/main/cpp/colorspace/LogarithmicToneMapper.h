//
// Created by Radzivon Bartoshyk on 11/10/2024.
//

#ifndef AVIF_LOGARITHMICTONEMAPPER_H
#define AVIF_LOGARITHMICTONEMAPPER_H

#include <vector>

class LogarithmicToneMapper {
public:
    LogarithmicToneMapper(const float primaries[3]) {
        std::copy(primaries, primaries + 3, lumaPrimaries);

        float Lmax = 1;
        float exposure = 1.f;
        den = static_cast<float>(1) / log(static_cast<float>(1 + Lmax * exposure));
    }

    void transferTone(float *inPlace, uint32_t width);

private:
    float lumaPrimaries[3] = {0};
    float den;
};


#endif //AVIF_LOGARITHMICTONEMAPPER_H
