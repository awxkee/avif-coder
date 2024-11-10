/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 01/01/2023
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

#ifndef AVIF_SIZESCALER_H
#define AVIF_SIZESCALER_H

#include <vector>
#include <jni.h>
#include "heif.h"
#include "definitions.h"

enum ScaleMode {
  Fit = 1,
  Fill = 2,
  Resize = 3,
};

bool RescaleImage(aligned_uint8_vector &initialData,
                  std::shared_ptr<heif_image_handle> &handle,
                  std::shared_ptr<heif_image> &img,
                  int *stride,
                  bool useFloats,
                  int *imageWidthPtr, int *imageHeightPtr,
                  int scaledWidth, int scaledHeight, ScaleMode scaleMode, int scalingQuality,
                  bool isRgba);

aligned_uint8_vector RescaleSourceImage(uint8_t *data,
                                        uint32_t *stride,
                                        uint32_t bitDepth,
                                        bool isImage64Bits,
                                        uint32_t *imageWidthPtr,
                                        uint32_t *imageHeightPtr,
                                        uint32_t scaledWidth,
                                        uint32_t scaledHeight,
                                        ScaleMode scaleMode,
                                        int scalingQuality,
                                        bool isRgba);

std::pair<uint32_t, uint32_t>
ResizeAspectFit(std::pair<uint32_t, uint32_t> sourceSize,
                std::pair<uint32_t, uint32_t> dstSize,
                float *scale);

std::pair<uint32_t, uint32_t>
ResizeAspectFill(std::pair<uint32_t, uint32_t> sourceSize,
                 std::pair<uint32_t, uint32_t> dstSize,
                 float *scale);

std::pair<uint32_t, uint32_t>
ResizeAspectHeight(std::pair<uint32_t, uint32_t> sourceSize, uint32_t maxHeight, bool multipleBy2);

std::pair<uint32_t, uint32_t>
ResizeAspectWidth(std::pair<uint32_t, uint32_t> sourceSize, uint32_t maxWidth, bool multipleBy2);

#endif //AVIF_SIZESCALER_H
