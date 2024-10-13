/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 16/9/2023
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

#include <android/log.h>
#include "IccRecognizer.h"

void RecognizeICC(std::shared_ptr<heif_image_handle> &handle,
                  std::shared_ptr<heif_image> &image,
                  std::vector<uint8_t> &iccProfile,
                  std::string &colorSpaceName,
                  heif_color_profile_nclx **nclx,
                  bool *hasNclx) {

  auto type = heif_image_get_color_profile_type(image.get());

  auto nclxColorProfile = heif_image_handle_get_nclx_color_profile(handle.get(),
                                                                   nclx);

  if (type == heif_color_profile_type_prof || type == heif_color_profile_type_rICC) {
    auto profileSize = heif_image_get_raw_color_profile_size(image.get());
    if (profileSize > 0) {
      iccProfile.resize(profileSize);
      auto iccStatus = heif_image_get_raw_color_profile(image.get(), iccProfile.data());
      if (iccStatus.code != heif_error_Ok) {
        iccProfile.resize(0);
      }
    }
  } else if (nclxColorProfile.code == heif_error_Ok) {
    *hasNclx = true;
    auto colorProfileNclx = *nclx;
    if (colorProfileNclx && colorProfileNclx->color_primaries != 0 &&
        colorProfileNclx->transfer_characteristics != 0) {
      auto transfer = colorProfileNclx->transfer_characteristics;
      auto colorPrimaries = colorProfileNclx->color_primaries;
      if (colorPrimaries == heif_color_primaries_ITU_R_BT_2020_2_and_2100_0 &&
          transfer == heif_transfer_characteristic_ITU_R_BT_2100_0_PQ) {
        colorSpaceName = "BT2020_PQ";
      } else if (colorPrimaries == heif_color_primaries_ITU_R_BT_709_5 &&
          transfer == heif_transfer_characteristic_linear) {
        colorSpaceName = "LINEAR_SRGB";
      } else if (colorPrimaries == heif_color_primaries_ITU_R_BT_2020_2_and_2100_0 &&
          transfer == heif_transfer_characteristic_ITU_R_BT_2100_0_HLG) {
        colorSpaceName = "BT2020_HLG";
      } else if (colorPrimaries == heif_color_primaries_ITU_R_BT_709_5 &&
          transfer == heif_transfer_characteristic_ITU_R_BT_709_5) {
        colorSpaceName = "BT709";
      } else if ((transfer == heif_transfer_characteristic_ITU_R_BT_2020_2_10bit ||
          transfer == heif_transfer_characteristic_ITU_R_BT_2020_2_12bit)) {
        colorSpaceName = "BT2020";
      } else if (colorPrimaries == heif_color_primaries_SMPTE_EG_432_1 &&
          transfer == heif_transfer_characteristic_ITU_R_BT_2100_0_HLG) {
        colorSpaceName = "DISPLAY_P3_HLG";
      } else if (colorPrimaries == heif_color_primaries_SMPTE_EG_432_1 &&
          transfer == heif_transfer_characteristic_ITU_R_BT_2100_0_PQ) {
        colorSpaceName = "DISPLAY_P3_PQ";
      } else if (colorPrimaries == heif_color_primaries_SMPTE_EG_432_1 &&
          transfer == heif_transfer_characteristic_ITU_R_BT_709_5) {
        colorSpaceName = "DISPLAY_P3";
      } else if (colorPrimaries == heif_color_primaries_ITU_R_BT_2020_2_and_2100_0) {
        colorSpaceName = "BT2020";
      } else if (transfer == heif_transfer_characteristic_SMPTE_ST_428_1) {
        colorSpaceName = "SMPTE_428";
      } else if (transfer == heif_transfer_characteristic_ITU_R_BT_2100_0_PQ) {
        colorSpaceName = "BT2020_PQ";
      } else if (transfer == heif_transfer_characteristic_ITU_R_BT_2100_0_HLG) {
        colorSpaceName = "BT2020_HLG";
      }
    }
  }
}