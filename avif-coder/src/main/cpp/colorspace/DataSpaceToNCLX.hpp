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
#include "GamutAdapter.h"
#include "colorspace.h"

namespace coder {
bool colorProfileFromDataSpace(uint8_t *data,
                               const int stride, const int width, const int height,
                               const bool dataTypeUnsigned8, const int bitDepth,
                               const int dataSpace, heif_color_profile_nclx *profile,
                               std::vector<uint8_t> &iccProfile) {
  if (dataSpace == -1) {
    return false;
  }

  bool isResolved = false;

  if (dataSpace == ADataSpace::ADATASPACE_UNKNOWN || dataSpace == ADataSpace::ADATASPACE_SCRGB) {
    profile->transfer_characteristics = heif_transfer_characteristic_IEC_61966_2_1;
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_601_6;
    profile->color_primaries = heif_color_primaries_ITU_R_BT_601_6;
    if (dataSpace == ADataSpace::ADATASPACE_SCRGB) {
      profile->full_range_flag = true;
    }
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_UNKNOWN || dataSpace == ADataSpace::ADATASPACE_BT601_525
      || dataSpace == ADataSpace::ADATASPACE_BT601_625) {
    profile->transfer_characteristics = heif_transfer_characteristic_ITU_R_BT_601_6;
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_601_6;
    profile->color_primaries = heif_color_primaries_ITU_R_BT_601_6;
    if (dataSpace == ADataSpace::ADATASPACE_JFIF) {
      profile->full_range_flag = true;
    }
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_BT709) {
    profile->transfer_characteristics = heif_transfer_characteristic_ITU_R_BT_709_5;
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_709_5;
    profile->color_primaries = heif_color_primaries_ITU_R_BT_709_5;
    profile->full_range_flag = false;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_SRGB) {
    profile->transfer_characteristics = heif_transfer_characteristic_IEC_61966_2_1;
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_709_5;
    profile->color_primaries = heif_color_primaries_ITU_R_BT_709_5;
    profile->full_range_flag = true;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_BT2020) {
    profile->transfer_characteristics = heif_transfer_characteristic_ITU_R_BT_2020_2_10bit;
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_2020_2_non_constant_luminance;
    profile->color_primaries = heif_color_primaries_ITU_R_BT_2020_2_and_2100_0;
    profile->full_range_flag = true;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_DISPLAY_P3) {
    profile->transfer_characteristics = heif_transfer_characteristic_IEC_61966_2_1;
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_601_6;
    profile->color_primaries = heif_color_primaries_SMPTE_EG_432_1;
    profile->full_range_flag = true;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_DCI_P3) {
    auto dciP3Profile = colorspacesCreateDCIP3RgbProfile();
    auto icc = dciP3Profile.iccProfile();
    iccProfile = icc;
    profile->full_range_flag = true;
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_SCRGB_LINEAR) {
    profile->transfer_characteristics = heif_transfer_characteristic_linear;
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_709_5;
    profile->color_primaries = heif_color_primaries_ITU_R_BT_709_5;
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
    if (dataSpace == ADataSpace::ADATASPACE_BT2020_HLG || dataSpace == ADataSpace::ADATASPACE_BT2020_PQ) {
      profile->full_range_flag = true;
    }
    isResolved = true;
  } else if (dataSpace == ADataSpace::ADATASPACE_ADOBE_RGB) {
    auto adobe = colorspacesCreateAdobergbProfile();
    auto icc = adobe.iccProfile();
    iccProfile = icc;
    isResolved = true;
  }

  return isResolved;
}
}