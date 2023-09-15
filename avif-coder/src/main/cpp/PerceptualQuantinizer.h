//
// Created by Radzivon Bartoshyk on 07/09/2023.
//

#ifndef AVIF_PERCEPTUALQUANTINIZER_H
#define AVIF_PERCEPTUALQUANTINIZER_H

#include <cstdint>

class PerceptualQuantinizer {
public:
    PerceptualQuantinizer(uint8_t *rgbaData, int stride, int width, int height, bool U16, bool halfFloats, int bitDepth) {
        this->bitDepth = bitDepth;
        this->halfFloats = halfFloats;
        this->rgbaData = rgbaData;
        this->stride = stride;
        this->width = width;
        this->height = height;
        this->U16 = U16;
    }

    void transfer();
private:
    bool halfFloats;
    int stride;
    int bitDepth;
    int width;
    int height;
    uint8_t *rgbaData;
    bool U16;
#if HAVE_NEON
    void transferNEONF16();
    void transferNEONU8();
#endif
protected:
    bool mayUseFPU = true;
};


#endif //AVIF_PERCEPTUALQUANTINIZER_H
