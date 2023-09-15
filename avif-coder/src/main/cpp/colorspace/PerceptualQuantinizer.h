//
// Created by Radzivon Bartoshyk on 07/09/2023.
//

#ifndef AVIF_PERCEPTUALQUANTINIZER_H
#define AVIF_PERCEPTUALQUANTINIZER_H

#include <cstdint>

enum PQGammaCorrection {
    Rec2020, DCIP3
};

class PerceptualQuantinizer {
public:
    PerceptualQuantinizer(uint8_t *rgbaData, int stride, int width, int height, bool U16,
                          bool halfFloats, int bitDepth, PQGammaCorrection gammaCorrection) {
        this->bitDepth = bitDepth;
        this->halfFloats = halfFloats;
        this->rgbaData = rgbaData;
        this->stride = stride;
        this->width = width;
        this->height = height;
        this->U16 = U16;
        this->gammaCorrection = gammaCorrection;
    }

    void transfer();

private:
    bool halfFloats;
    int stride;
    int bitDepth;
    int width;
    int height;
    uint8_t *rgbaData;
    PQGammaCorrection gammaCorrection;
    bool U16;
protected:
};


#endif //AVIF_PERCEPTUALQUANTINIZER_H
