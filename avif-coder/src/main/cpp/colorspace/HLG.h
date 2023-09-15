//
// Created by Radzivon Bartoshyk on 15/09/2023.
//

#ifndef AVIF_HLG_H
#define AVIF_HLG_H

#include <cstdint>

namespace coder {

    enum HLGGammaCorrection {
        Rec2020, DCIP3
    };

    void ProcessHLG(uint8_t *data, bool halfFloats, int stride, int width, int height, int depth,
                    HLGGammaCorrection correction);
}

#endif //AVIF_HLG_H
