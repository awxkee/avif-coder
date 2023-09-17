//
// Created by Radzivon Bartoshyk on 01/01/2023.
//

#ifndef AVIF_SIZESCALER_H
#define AVIF_SIZESCALER_H

#include <vector>
#include <jni.h>
#include "heif.h"

enum ScaleMode {
    Fit = 1,
    Fill = 2,
    Resize = 3,
};

bool RescaleImage(std::vector<uint8_t> &initialData,
                  JNIEnv *env,
                  heif_image_handle *handle,
                  heif_image *img,
                  int *stride,
                  bool useFloats,
                  int *imageWidthPtr, int *imageHeightPtr,
                  int scaledWidth, int scaledHeight, ScaleMode scaleMode);

std::pair<int, int>
ResizeAspectFit(std::pair<int, int> sourceSize, std::pair<int, int> dstSize, float *scale);

std::pair<int, int>
ResizeAspectFill(std::pair<int, int> sourceSize, std::pair<int, int> dstSize, float *scale);

std::pair<int, int>
ResizeAspectHeight(std::pair<int, int> sourceSize, int maxHeight, bool multipleBy2);

std::pair<int, int>
ResizeAspectWidth(std::pair<int, int> sourceSize, int maxWidth, bool multipleBy2);

#endif //AVIF_SIZESCALER_H
