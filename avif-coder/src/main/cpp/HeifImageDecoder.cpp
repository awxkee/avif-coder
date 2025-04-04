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

#include "HeifImageDecoder.h"
#include <thread>
#include "IccRecognizer.h"
#include "colorspace.h"
#include "Eigen/Eigen"
#include "ColorSpaceProfile.h"
#include <Trc.h>
#include <ITUR.h>
#include "ColorMatrix.h"
#include "avifweaver.h"

AvifImageFrame HeifImageDecoder::getFrame(std::vector<uint8_t> &srcBuffer,
                                          uint32_t scaledWidth,
                                          uint32_t scaledHeight,
                                          PreferredColorConfig javaColorSpace,
                                          ScaleMode javaScaleMode,
                                          int scalingQuality) {
  heif_context_set_max_decoding_threads(ctx.get(), (int) std::thread::hardware_concurrency());

  auto result = heif_context_read_from_memory_without_copy(ctx.get(), srcBuffer.data(),
                                                           srcBuffer.size(),
                                                           nullptr);
  if (result.code != heif_error_Ok) {
    throw std::runtime_error("Can't read heif file exception");
  }

  heif_image_handle *handlePtr;
  result = heif_context_get_primary_image_handle(ctx.get(), &handlePtr);
  if (result.code != heif_error_Ok || handlePtr == nullptr) {
    throw std::runtime_error("Acquiring an image from file has failed");
  }

  std::shared_ptr<heif_image_handle> handle(handlePtr, [](heif_image_handle *hd) {
    heif_image_handle_release(hd);
  });

  int bitDepth = heif_image_handle_get_chroma_bits_per_pixel(handle.get());
  bool useBitmapHalf16Floats = bitDepth > 8;
  if (bitDepth < 0) {
    std::string currentBitDepthNotSupported =
        "Stored bit depth in an image is not supported: " + std::to_string(bitDepth);
    throw std::runtime_error(currentBitDepthNotSupported);
  }

  heif_image *imgPtr;
  std::unique_ptr<heif_decoding_options, HeifUniquePtrDeleter>
      options(heif_decoding_options_alloc());
  options->convert_hdr_to_8bit = false;
  options->ignore_transformations = false;
  result = heif_decode_image(handle.get(), &imgPtr, heif_colorspace_RGB,
                             useBitmapHalf16Floats ? heif_chroma_interleaved_RRGGBBAA_LE
                                                   : heif_chroma_interleaved_RGBA,
                             options.get());
  options.reset();

  if (result.code != heif_error_Ok || imgPtr == nullptr) {
    throw std::runtime_error("Decoding an image has failed");
  }

  std::shared_ptr<heif_image> img(imgPtr, [](heif_image *im) {
    heif_image_release(im);
  });

  float intensityTarget = 1000.0f;

  if (heif_image_has_content_light_level(img.get())) {
    heif_content_light_level light_level = {0};
    heif_image_get_content_light_level(img.get(), &light_level);
    if (light_level.max_content_light_level != 0) {
      intensityTarget = light_level.max_content_light_level;
    }
  }

  std::vector<uint8_t> profile;
  std::string colorProfile;

  heif_color_profile_nclx *nclx = nullptr;
  bool hasNCLX = false;

  RecognizeICC(handle, img, profile, colorProfile, &nclx, &hasNCLX);

  bool imageHasAlpha = heif_image_handle_has_alpha_channel(handle.get());

  int imageWidth;
  int imageHeight;
  int stride;
  aligned_uint8_vector initialData;

  imageWidth = heif_image_get_width(img.get(), heif_channel_interleaved);
  imageHeight = heif_image_get_height(img.get(), heif_channel_interleaved);
  auto scaleResult = RescaleImage(initialData,
                                  handle,
                                  img,
                                  &stride,
                                  useBitmapHalf16Floats,
                                  &imageWidth,
                                  &imageHeight,
                                  static_cast<int>(scaledWidth),
                                  static_cast<int>(scaledHeight),
                                  javaScaleMode,
                                  scalingQuality,
                                  imageHasAlpha);
  if (!scaleResult) {
    throw std::runtime_error("Rescaling an image has failed");
  }

  img.reset();
  handle.reset();

  aligned_uint8_vector dstARGB = initialData;

  initialData.clear();

  if (!profile.empty()) {
    convertUseICC(dstARGB, stride, imageWidth, imageHeight, profile.data(),
                  profile.size(),
                  useBitmapHalf16Floats, bitDepth);
  } else if (hasNCLX && nclx &&
      nclx->transfer_characteristics != heif_transfer_characteristic_unspecified &&
      nclx->color_primaries != heif_color_primaries_unspecified) {
    Eigen::Matrix<float, 3, 2> primaries;
    if (nclx->color_primaries != heif_color_primaries_unspecified) {
      primaries << static_cast<float>(nclx->color_primary_red_x),
          static_cast<float>(nclx->color_primary_red_y),
          static_cast<float>(nclx->color_primary_green_x),
          static_cast<float>(nclx->color_primary_green_y),
          static_cast<float>(nclx->color_primary_blue_x),
          static_cast<float>(nclx->color_primary_blue_y);
    } else {
      primaries = getSRGBPrimaries();
    }
    Eigen::Vector2f whitePoint;
    if (nclx->color_primaries != heif_color_primaries_unspecified) {
      whitePoint << static_cast<float>(nclx->color_primary_white_x),
          static_cast<float>(nclx->color_primary_white_y);
    } else {
      whitePoint = getIlluminantD65();
    }

    Eigen::Matrix3f destinationProfile = GamutRgbToXYZ(getSRGBPrimaries(), getIlluminantD65());
    Eigen::Matrix3f sourceProfile = GamutRgbToXYZ(primaries, whitePoint);

    Eigen::Matrix3f conversion = destinationProfile.inverse() * sourceProfile;

    ToneMapping toneMapping = ToneMapping::Rec2408;

    if (nclx->transfer_characteristics !=
        heif_transfer_characteristic_ITU_R_BT_2100_0_HLG &&
        nclx->transfer_characteristics != heif_transfer_characteristic_ITU_R_BT_2100_0_PQ) {
      toneMapping = ToneMapping::Skip;
    }

    FfiTrc transferFfi = FfiTrc::Srgb;
    TransferFunction forwardTrc = TransferFunction::Srgb;

    if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_ITU_R_BT_2100_0_HLG) {
      transferFfi = FfiTrc::Hlg;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_SMPTE_ST_428_1) {
      transferFfi = FfiTrc::Smpte428;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_ITU_R_BT_2100_0_PQ) {
      transferFfi = FfiTrc::Smpte2084;
    } else if (nclx->transfer_characteristics == heif_transfer_characteristic_linear) {
      transferFfi = FfiTrc::Linear;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_ITU_R_BT_470_6_System_M) {
      transferFfi = FfiTrc::Bt470M;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_ITU_R_BT_470_6_System_B_G) {
      transferFfi = FfiTrc::Bt470Bg;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_ITU_R_BT_601_6) {
      transferFfi = FfiTrc::Bt709;
    } else if (nclx->transfer_characteristics == heif_transfer_characteristic_ITU_R_BT_709_5) {
      transferFfi = FfiTrc::Bt709;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_ITU_R_BT_2020_2_10bit ||
        nclx->transfer_characteristics ==
            heif_transfer_characteristic_ITU_R_BT_2020_2_12bit) {
      transferFfi = FfiTrc::Bt709;
    } else if (nclx->transfer_characteristics == heif_transfer_characteristic_SMPTE_240M) {
      transferFfi = FfiTrc::Smpte240;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_logarithmic_100) {
      transferFfi = FfiTrc::Log100;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_logarithmic_100_sqrt10) {
      transferFfi = FfiTrc::Log100sqrt10;
    } else if (nclx->transfer_characteristics == heif_transfer_characteristic_IEC_61966_2_1) {
      transferFfi = FfiTrc::Srgb;
    } else if (nclx->transfer_characteristics == heif_transfer_characteristic_IEC_61966_2_4) {
      transferFfi = FfiTrc::Iec61966;
    } else if (nclx->transfer_characteristics == heif_transfer_characteristic_ITU_R_BT_1361) {
      transferFfi = FfiTrc::Bt1361;
    } else if (nclx->transfer_characteristics == heif_transfer_characteristic_unspecified) {
      transferFfi = FfiTrc::Srgb;
    }

    const float cPrimaries[6] = {
        primaries(0), primaries(1),
        primaries(2), primaries(3),
        primaries(4), primaries(5)
    };
    const float wp[2] = {
        whitePoint(0), whitePoint(1)
    };

    if (useBitmapHalf16Floats) {
      apply_tone_mapping_rgba16(
          reinterpret_cast<uint16_t *>(dstARGB.data()), stride, bitDepth,
          imageWidth, imageHeight, cPrimaries, wp, transferFfi, toneMapping, intensityTarget
      );
    } else {
      apply_tone_mapping_rgba8(
          reinterpret_cast<uint8_t *>(dstARGB.data()), stride,
          imageWidth, imageHeight, cPrimaries, wp, transferFfi, toneMapping, intensityTarget
      );
    }
  }

  AvifImageFrame imageFrame = {
      .store = dstARGB,
      .width = static_cast<uint32_t >(imageWidth),
      .height = static_cast<uint32_t >(imageHeight),
      .is16Bit = useBitmapHalf16Floats,
      .bitDepth = static_cast<uint32_t >(bitDepth),
      .hasAlpha = imageHasAlpha
  };
  return imageFrame;
}

std::string HeifImageDecoder::getImageType(std::vector<uint8_t> &srcBuffer) {
  auto cMime = heif_get_file_mime_type(reinterpret_cast<const uint8_t *>(srcBuffer.data()),
                                       static_cast<int>(srcBuffer.size()));
  if (!cMime) {
    std::string vec = "image/avif";
    return vec;
  }
  std::string mime(cMime);
  return mime;
}