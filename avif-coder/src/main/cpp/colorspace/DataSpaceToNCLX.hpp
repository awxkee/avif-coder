/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 07/03/2024
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

#pragma once

#include <android/data_space.h>
#include "libheif/heif.h"
#include "Eigen/Eigen"
#include "ColorSpaceProfile.h"
#include "colorspace.h"
#include "avifweaver.h"
#include "avif/avif.h"

std::vector<uint8_t> newAdobeRGBProfile() {
  auto new_ffi_profile = new_adobe_rgb_profile();
  std::vector<uint8_t> newVector;
  if (new_ffi_profile.size == 0 || new_ffi_profile.data == nullptr) {
    free_profile(new_ffi_profile);
    return newVector;
  }
  newVector.resize(new_ffi_profile.size);
  std::copy(new_ffi_profile.data, new_ffi_profile.data + new_ffi_profile.size, newVector.begin());
  free_profile(new_ffi_profile);
  return newVector;
}

std::vector<uint8_t> newDCIP3Profile() {
  auto new_ffi_profile = new_dci_p3_profile();
  std::vector<uint8_t> newVector;
  if (new_ffi_profile.size == 0 || new_ffi_profile.data == nullptr) {
    free_profile(new_ffi_profile);
    return newVector;
  }
  newVector.resize(new_ffi_profile.size);
  std::copy(new_ffi_profile.data, new_ffi_profile.data + new_ffi_profile.size, newVector.begin());
  free_profile(new_ffi_profile);
  return newVector;
}

namespace coder {

bool colorProfileFromDataSpace(const int dataSpace, heif_color_profile_nclx *profile,
                               std::vector<uint8_t> &iccProfile, YuvMatrix &matrix) {
  if (dataSpace == -1) {
    return false;
  }

  bool isResolved = false;

  if (dataSpace == ADataSpace::ADATASPACE_UNKNOWN || dataSpace == ADataSpace::ADATASPACE_SCRGB) {
    profile->transfer_characteristics = heif_transfer_characteristic_IEC_61966_2_1;
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_601_6;
    profile->color_primaries = heif_color_primaries_ITU_R_BT_601_6;
    matrix = YuvMatrix::Bt601;
    if (dataSpace == ADataSpace::ADATASPACE_SCRGB) {
      profile->full_range_flag = true;
    }
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_BT601_525
      || dataSpace == ADataSpace::ADATASPACE_BT601_625) {
    profile->transfer_characteristics = heif_transfer_characteristic_ITU_R_BT_601_6;
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_601_6;
    profile->color_primaries = heif_color_primaries_ITU_R_BT_601_6;
    matrix = YuvMatrix::Bt601;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_BT709) {
    profile->transfer_characteristics = heif_transfer_characteristic_ITU_R_BT_709_5;
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_709_5;
    profile->color_primaries = heif_color_primaries_ITU_R_BT_709_5;
    matrix = YuvMatrix::Bt709;
    profile->full_range_flag = false;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_SRGB) {
    profile->transfer_characteristics = heif_transfer_characteristic_IEC_61966_2_1;
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_709_5;
    profile->color_primaries = heif_color_primaries_ITU_R_BT_709_5;
    matrix = YuvMatrix::Bt709;
    profile->full_range_flag = true;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_BT2020) {
    profile->transfer_characteristics = heif_transfer_characteristic_ITU_R_BT_2020_2_10bit;
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_2020_2_non_constant_luminance;
    profile->color_primaries = heif_color_primaries_ITU_R_BT_2020_2_and_2100_0;
    matrix = YuvMatrix::Bt2020;
    profile->full_range_flag = true;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_DISPLAY_P3) {
    profile->transfer_characteristics = heif_transfer_characteristic_IEC_61966_2_1;
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_601_6;
    profile->color_primaries = heif_color_primaries_SMPTE_EG_432_1;
    matrix = YuvMatrix::Bt601;
    profile->full_range_flag = true;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_DCI_P3) {
    auto dciP3Profile = newDCIP3Profile();
    matrix = YuvMatrix::Bt709;
    iccProfile = dciP3Profile;
    profile->full_range_flag = true;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_SCRGB_LINEAR) {
    profile->transfer_characteristics = heif_transfer_characteristic_linear;
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_709_5;
    profile->color_primaries = heif_color_primaries_ITU_R_BT_709_5;
    matrix = YuvMatrix::Bt709;
    profile->full_range_flag = true;
    isResolved = true;
  } else if ((dataSpace == ADataSpace::ADATASPACE_BT2020_ITU_PQ ||
      dataSpace == ADataSpace::ADATASPACE_BT2020_PQ ||
      dataSpace == ADataSpace::ADATASPACE_BT2020_HLG ||
      dataSpace == ADataSpace::ADATASPACE_BT2020_ITU_HLG)) {
    heif_transfer_characteristics transfer = heif_transfer_characteristic_ITU_R_BT_2100_0_PQ;
    if (dataSpace == ADataSpace::ADATASPACE_BT2020_HLG ||
        dataSpace == ADataSpace::ADATASPACE_BT2020_ITU_HLG) {
      transfer = heif_transfer_characteristic_ITU_R_BT_2100_0_HLG;
    }
    profile->transfer_characteristics = transfer;
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_2020_2_non_constant_luminance;
    profile->color_primaries = heif_color_primaries_ITU_R_BT_2020_2_and_2100_0;
    matrix = YuvMatrix::Bt2020;
    if (dataSpace == ADataSpace::ADATASPACE_BT2020_HLG
        || dataSpace == ADataSpace::ADATASPACE_BT2020_PQ) {
      profile->full_range_flag = true;
    }
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_ADOBE_RGB) {
    auto adobe = newAdobeRGBProfile();
    matrix = YuvMatrix::Bt709;
    iccProfile = adobe;
    isResolved = true;
  }

  return isResolved;
}

bool colorProfileFromDataSpaceAvif(const int dataSpace,
                                   avifTransferCharacteristics &transfer,
                                   avifMatrixCoefficients &matrixCoefficients,
                                   avifColorPrimaries &colorPrimaries,
                                   avifRange &avifRange,
                                   std::vector<uint8_t> &iccProfile, YuvMatrix &matrix) {
  if (dataSpace == -1) {
    return false;
  }

  bool isResolved = false;

  avifRange = AVIF_RANGE_LIMITED;

  if (dataSpace == ADataSpace::ADATASPACE_UNKNOWN || dataSpace == ADataSpace::ADATASPACE_SCRGB) {
    transfer = AVIF_TRANSFER_CHARACTERISTICS_SRGB;
    matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT601;
    colorPrimaries = AVIF_COLOR_PRIMARIES_BT601;
    matrix = YuvMatrix::Bt601;
    if (dataSpace == ADataSpace::ADATASPACE_SCRGB) {
      avifRange = AVIF_RANGE_FULL;
    }
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_BT601_525
      || dataSpace == ADataSpace::ADATASPACE_BT601_625) {
    transfer = AVIF_TRANSFER_CHARACTERISTICS_BT601;
    matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT601;
    colorPrimaries = AVIF_COLOR_PRIMARIES_BT601;
    matrix = YuvMatrix::Bt601;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_BT709) {
    transfer = AVIF_TRANSFER_CHARACTERISTICS_BT709;
    matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT709;
    colorPrimaries = AVIF_COLOR_PRIMARIES_BT709;
    matrix = YuvMatrix::Bt709;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_SRGB) {
    transfer = AVIF_TRANSFER_CHARACTERISTICS_SRGB;
    matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT709;
    colorPrimaries = AVIF_COLOR_PRIMARIES_BT709;
    matrix = YuvMatrix::Bt709;
    avifRange = AVIF_RANGE_FULL;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_BT2020) {
    transfer = AVIF_TRANSFER_CHARACTERISTICS_BT2020_10BIT;
    matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT2020_NCL;
    colorPrimaries = AVIF_COLOR_PRIMARIES_BT2100;
    matrix = YuvMatrix::Bt2020;
    avifRange = AVIF_RANGE_FULL;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_DISPLAY_P3) {
    transfer = AVIF_TRANSFER_CHARACTERISTICS_SRGB;
    matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT601;
    colorPrimaries = AVIF_COLOR_PRIMARIES_SMPTE432;
    matrix = YuvMatrix::Bt601;
    avifRange = AVIF_RANGE_FULL;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_DCI_P3) {
    auto dciP3Profile = newDCIP3Profile();
    matrix = YuvMatrix::Bt709;
    iccProfile = dciP3Profile;
    avifRange = AVIF_RANGE_FULL;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_SCRGB_LINEAR) {
    transfer = AVIF_TRANSFER_CHARACTERISTICS_LINEAR;
    matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT709;
    colorPrimaries = AVIF_COLOR_PRIMARIES_BT709;
    matrix = YuvMatrix::Bt709;
    avifRange = AVIF_RANGE_FULL;
    isResolved = true;
  } else if ((dataSpace == ADataSpace::ADATASPACE_BT2020_ITU_PQ ||
      dataSpace == ADataSpace::ADATASPACE_BT2020_PQ ||
      dataSpace == ADataSpace::ADATASPACE_BT2020_HLG ||
      dataSpace == ADataSpace::ADATASPACE_BT2020_ITU_HLG)) {
    avifTransferCharacteristics vTransfer = AVIF_TRANSFER_CHARACTERISTICS_PQ;
    if (dataSpace == ADataSpace::ADATASPACE_BT2020_HLG ||
        dataSpace == ADataSpace::ADATASPACE_BT2020_ITU_HLG) {
      vTransfer = AVIF_TRANSFER_CHARACTERISTICS_HLG;
    }
    transfer = vTransfer;
    matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT2020_NCL;
    colorPrimaries = AVIF_COLOR_PRIMARIES_BT2100;
    matrix = YuvMatrix::Bt2020;
    if (dataSpace == ADataSpace::ADATASPACE_BT2020_HLG
        || dataSpace == ADataSpace::ADATASPACE_BT2020_PQ) {
      avifRange = AVIF_RANGE_FULL;
    }
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_ADOBE_RGB) {
    auto adobe = newAdobeRGBProfile();
    matrix = YuvMatrix::Bt709;
    iccProfile = adobe;
    isResolved = true;
  }

  return isResolved;
}
}