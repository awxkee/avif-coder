//
// Created by Radzivon Bartoshyk on 16/09/2023.
//

#ifndef AVIF_ICCRECOGNIZER_H
#define AVIF_ICCRECOGNIZER_H

#include <vector>
#include <string>
#include "heif.h"

void RecognizeICC(heif_image_handle* handle,
                  heif_image *image,
                  std::vector<uint8_t> &iccProfile,
                  std::string &colorSpaceName);

#endif //AVIF_ICCRECOGNIZER_H
