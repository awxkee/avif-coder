//
// Created by Radzivon Bartoshyk on 13/10/2024.
//

#include "HeifImageDecoder.h"
#include <thread>
#include "IccRecognizer.h"
#include "colorspace.h"
#include "Eigen/Eigen"
#include "ColorSpaceProfile.h"
#include <Trc.h>
#include <ITUR.h>
#include "ColorMatrix.h"

AvifImageFrame HeifImageDecoder::getFrame(std::vector<uint8_t> &srcBuffer,
                                          uint32_t scaledWidth,
                                          uint32_t scaledHeight,
                                          PreferredColorConfig javaColorSpace,
                                          ScaleMode javaScaleMode,
                                          CurveToneMapper toneMapper,
                                          bool highQualityResizer) {
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
                                  highQualityResizer);
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
                  useBitmapHalf16Floats);
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

    if (nclx->transfer_characteristics !=
        heif_transfer_characteristic_ITU_R_BT_2100_0_HLG &&
        nclx->transfer_characteristics != heif_transfer_characteristic_ITU_R_BT_2100_0_PQ) {
      toneMapper = TONE_SKIP;
    }

    TransferFunction forwardTrc = TransferFunction::Srgb;

    if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_ITU_R_BT_2100_0_HLG) {
      forwardTrc = TransferFunction::Hlg;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_SMPTE_ST_428_1) {
      forwardTrc = TransferFunction::Smpte428;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_ITU_R_BT_2100_0_PQ) {
      forwardTrc = TransferFunction::Pq;
    } else if (nclx->transfer_characteristics == heif_transfer_characteristic_linear) {
      forwardTrc = TransferFunction::Linear;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_ITU_R_BT_470_6_System_M) {
      forwardTrc = TransferFunction::Gamma2p2;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_ITU_R_BT_470_6_System_B_G) {
      forwardTrc = TransferFunction::Gamma2p8;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_ITU_R_BT_601_6) {
      forwardTrc = TransferFunction::Itur709;
    } else if (nclx->transfer_characteristics == heif_transfer_characteristic_ITU_R_BT_709_5) {
      forwardTrc = TransferFunction::Itur709;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_ITU_R_BT_2020_2_10bit ||
        nclx->transfer_characteristics ==
            heif_transfer_characteristic_ITU_R_BT_2020_2_12bit) {
      forwardTrc = TransferFunction::Itur709;
    } else if (nclx->transfer_characteristics == heif_transfer_characteristic_SMPTE_240M) {
      forwardTrc = TransferFunction::Smpte240;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_logarithmic_100) {
      forwardTrc = TransferFunction::Log100;
    } else if (nclx->transfer_characteristics ==
        heif_transfer_characteristic_logarithmic_100_sqrt10) {
      forwardTrc = TransferFunction::Log100Sqrt10;
    } else if (nclx->transfer_characteristics == heif_transfer_characteristic_IEC_61966_2_1) {
      forwardTrc = TransferFunction::Srgb;
    } else if (nclx->transfer_characteristics == heif_transfer_characteristic_IEC_61966_2_4) {
      forwardTrc = TransferFunction::Iec61966;
    } else if (nclx->transfer_characteristics == heif_transfer_characteristic_ITU_R_BT_1361) {
      forwardTrc = TransferFunction::Bt1361;
    } else if (nclx->transfer_characteristics == heif_transfer_characteristic_unspecified) {
      forwardTrc = TransferFunction::Srgb;
    }

    ITURColorCoefficients coeffs = colorPrimariesComputeYCoeffs(primaries, whitePoint);

    const float matrix[9] = {
        conversion(0, 0), conversion(0, 1), conversion(0, 2),
        conversion(1, 0), conversion(1, 1), conversion(1, 2),
        conversion(2, 0), conversion(2, 1), conversion(2, 2),
    };

    if (useBitmapHalf16Floats) {
      applyColorMatrix16Bit(reinterpret_cast<uint16_t *>(dstARGB.data()),
                            stride,
                            imageWidth,
                            imageHeight,
                            bitDepth,
                            matrix,
                            forwardTrc,
                            TransferFunction::Srgb,
                            toneMapper,
                            coeffs);
    } else {
      applyColorMatrix(reinterpret_cast<uint8_t *>(dstARGB.data()), stride, imageWidth,
                       imageHeight, matrix, forwardTrc, TransferFunction::Srgb, toneMapper,
                       coeffs);
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
                                       srcBuffer.size());
  std::string mime(cMime);
  return mime;
}