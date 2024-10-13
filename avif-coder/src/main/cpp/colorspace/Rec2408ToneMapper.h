/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 13/10/2024
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

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
