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

#include "AvifDecoderController.h"
#include "avif/avif.h"
#include <exception>
#include <thread>
#include "imagebits/CopyUnalignedRGBA.h"
#include "colorspace.h"
#include "Eigen/Eigen"
#include "ColorSpaceProfile.h"
#include "avifweaver.h"
#include <android/log.h>
#include "JniException.h"
#include <cmath>

class AvifUniqueImage {
 public:
  avifRGBImage rgbImage;

  AvifUniqueImage(avifDecoder *decoder) {
    rgbImage = {0};
    avifRGBImageSetDefaults(&rgbImage, decoder->image);
    rgbImage.format = AVIF_RGB_FORMAT_RGBA;
  }

  ~AvifUniqueImage() {
    if (isPlanesAllocated) {
      avifRGBImageFreePixels(&rgbImage);
      isPlanesAllocated = false;
    }
  }

  avifResult allocateImage() {
    auto result = avifRGBImageAllocatePixels(&rgbImage);
    if (result == AVIF_RESULT_OK) {
      this->isPlanesAllocated = true;
    }
    return result;
  }

  void clear() {
    if (isPlanesAllocated) {
      avifRGBImageFreePixels(&rgbImage);
      isPlanesAllocated = false;
    }
  }

 private:
  bool isPlanesAllocated = false;
};

AvifImageFrame AvifDecoderController::getFrame(uint32_t frame,
                                               int32_t scaledWidth,
                                               int32_t scaledHeight,
                                               PreferredColorConfig javaColorSpace,
                                               ScaleMode javaScaleMode,
                                               int scalingQuality) {
  std::lock_guard guard(this->mutex);
  if (!this->isBufferAttached) {
    throw std::runtime_error("AVIF controller methods can't be called without attached buffer");
  }

  if (frame >= this->decoder->imageCount) {
    std::string str = "Can't time of frame number: " + std::to_string(frame);
    throw std::runtime_error(str);
  }

  avifResult nextImageResult = avifDecoderNthImage(this->decoder.get(), frame);
  if (nextImageResult != AVIF_RESULT_OK) {
    std::string str = "Can't time of frame number: " + std::to_string(frame);
    throw std::runtime_error(str);
  }

  AvifUniqueImage avifUniqueImage(this->decoder.get());

  auto
      imageUsesAlpha = decoder->image->imageOwnsAlphaPlane || decoder->image->alphaPlane != nullptr;

  auto colorPrimaries = decoder->image->colorPrimaries;
  auto transferCharacteristics = decoder->image->transferCharacteristics;

  uint32_t bitDepth = decoder->image->depth;

  bool isImageRequires64Bit = avifImageUsesU16(decoder->image);
  if (isImageRequires64Bit) {
    avifUniqueImage.rgbImage.alphaPremultiplied = false;
    avifUniqueImage.rgbImage.depth = bitDepth;
  } else {
    avifUniqueImage.rgbImage.alphaPremultiplied = false;
    avifUniqueImage.rgbImage.depth = 8;
  }

  avifResult rgbResult = avifUniqueImage.allocateImage();
  if (rgbResult != AVIF_RESULT_OK) {
    std::string
        str = "Can't correctly allocate buffer for frame with numbers: " + std::to_string(frame);
    throw std::runtime_error(str);
  }

  bool isImageConverted = false;

  auto image = decoder->image;

  auto type = decoder->image->yuvFormat;

  YuvMatrix matrix = YuvMatrix::Bt709;
  if (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_BT601) {
    matrix = YuvMatrix::Bt601;
  } else if (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_BT2020_NCL
      || image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_SMPTE2085) {
    matrix = YuvMatrix::Bt2020;
  } else if (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY) {
    matrix = YuvMatrix::Identity;
    if (type != AVIF_PIXEL_FORMAT_YUV444) {
      std::string
          str = "On identity matrix image layout must be 4:4:4 but it wasn't";
      throw std::runtime_error(str);
    }
  } else if (image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO) {
    matrix = YuvMatrix::YCgCo;
  }

  YuvRange range = YuvRange::Tv;
  if (image->yuvRange == AVIF_RANGE_FULL) {
    range = YuvRange::Pc;
  }

  YuvType yuvType = YuvType::Yuv420;
  if (type == AVIF_PIXEL_FORMAT_YUV422) {
    yuvType = YuvType::Yuv422;
  } else if (type == AVIF_PIXEL_FORMAT_YUV444) {
    yuvType = YuvType::Yuv444;
  }

  if (type == AVIF_PIXEL_FORMAT_YUV444 || type == AVIF_PIXEL_FORMAT_YUV422
      || type == AVIF_PIXEL_FORMAT_YUV420) {

    if (isImageRequires64Bit) {
      if (imageUsesAlpha) {
        weave_yuv16_with_alpha_to_rgba16(
            reinterpret_cast<const uint16_t *>(image->yuvPlanes[0]),
            image->yuvRowBytes[0],
            reinterpret_cast<const uint16_t *>(image->yuvPlanes[1]),
            image->yuvRowBytes[1],
            reinterpret_cast<const uint16_t *>(image->yuvPlanes[2]),
            image->yuvRowBytes[2],
            reinterpret_cast<const uint16_t *>(image->alphaPlane),
            image->alphaRowBytes,
            reinterpret_cast<uint16_t *>(avifUniqueImage.rgbImage.pixels),
            avifUniqueImage.rgbImage.rowBytes,
            bitDepth,
            avifUniqueImage.rgbImage.width,
            avifUniqueImage.rgbImage.height,
            range,
            matrix,
            yuvType
        );
        isImageConverted = true;
      } else {
        weave_yuv16_to_rgba16(
            reinterpret_cast<const uint16_t *>(image->yuvPlanes[0]),
            image->yuvRowBytes[0],
            reinterpret_cast<const uint16_t *>(image->yuvPlanes[1]),
            image->yuvRowBytes[1],
            reinterpret_cast<const uint16_t *>(image->yuvPlanes[2]),
            image->yuvRowBytes[2],
            reinterpret_cast<uint16_t *>(avifUniqueImage.rgbImage.pixels),
            avifUniqueImage.rgbImage.rowBytes,
            bitDepth,
            avifUniqueImage.rgbImage.width,
            avifUniqueImage.rgbImage.height,
            range,
            matrix,
            yuvType
        );
        isImageConverted = true;
      }
    } else {
      if (imageUsesAlpha) {
        weave_yuv8_with_alpha_to_rgba8(
            image->yuvPlanes[0], image->yuvRowBytes[0],
            image->yuvPlanes[1], image->yuvRowBytes[1],
            image->yuvPlanes[2], image->yuvRowBytes[2],
            image->alphaPlane, image->alphaRowBytes,
            avifUniqueImage.rgbImage.pixels, avifUniqueImage.rgbImage.rowBytes,
            avifUniqueImage.rgbImage.width, avifUniqueImage.rgbImage.height,
            range, matrix, yuvType
        );
        isImageConverted = true;
      } else {
        weave_yuv8_to_rgba8(
            image->yuvPlanes[0], image->yuvRowBytes[0],
            image->yuvPlanes[1], image->yuvRowBytes[1],
            image->yuvPlanes[2], image->yuvRowBytes[2],
            avifUniqueImage.rgbImage.pixels, avifUniqueImage.rgbImage.rowBytes,
            avifUniqueImage.rgbImage.width, avifUniqueImage.rgbImage.height,
            range, matrix, yuvType
        );
        isImageConverted = true;
      }
    }
  } else if (type == AVIF_PIXEL_FORMAT_YUV400) {
    if (isImageRequires64Bit) {
      if (imageUsesAlpha) {
        weave_yuv400_p16_with_alpha_to_rgba16(
            reinterpret_cast<const uint16_t *>(image->yuvPlanes[0]),
            image->yuvRowBytes[0],
            reinterpret_cast<const uint16_t *>(image->alphaPlane),
            image->alphaRowBytes,
            reinterpret_cast<uint16_t *>(avifUniqueImage.rgbImage.pixels),
            avifUniqueImage.rgbImage.rowBytes,
            bitDepth,
            avifUniqueImage.rgbImage.width,
            avifUniqueImage.rgbImage.height,
            range,
            matrix
        );
      } else {
        weave_yuv400_p16_to_rgba16(
            reinterpret_cast<const uint16_t *>(image->yuvPlanes[0]),
            image->yuvRowBytes[0],
            reinterpret_cast<uint16_t *>(avifUniqueImage.rgbImage.pixels),
            avifUniqueImage.rgbImage.rowBytes,
            bitDepth,
            avifUniqueImage.rgbImage.width,
            avifUniqueImage.rgbImage.height,
            range,
            matrix
        );
      }
      isImageConverted = true;
    } else {
      if (imageUsesAlpha) {
        weave_yuv400_with_alpha_to_rgba8(
            reinterpret_cast<const uint8_t *>(image->yuvPlanes[0]),
            image->yuvRowBytes[0],
            image->alphaPlane, image->alphaRowBytes,
            reinterpret_cast<uint8_t *>(avifUniqueImage.rgbImage.pixels),
            avifUniqueImage.rgbImage.rowBytes,
            avifUniqueImage.rgbImage.width,
            avifUniqueImage.rgbImage.height,
            range,
            matrix
        );
      } else {
        weave_yuv400_to_rgba8(
            reinterpret_cast<const uint8_t *>(image->yuvPlanes[0]),
            image->yuvRowBytes[0],
            reinterpret_cast<uint8_t *>(avifUniqueImage.rgbImage.pixels),
            avifUniqueImage.rgbImage.rowBytes,
            avifUniqueImage.rgbImage.width,
            avifUniqueImage.rgbImage.height,
            range,
            matrix
        );
      }
      isImageConverted = true;
    }
  }

  if (!isImageConverted) {
    std::string
        str = "Unfortunately image type is not supported" + std::to_string(frame);
    throw std::runtime_error(str);
  }

  float intensityTarget =
      decoder->image->clli.maxCLL == 0 ? 1000.0f : static_cast<float>(decoder->image->clli.maxCLL);

  aligned_uint8_vector iccProfile(0);
  if (decoder->image->icc.data && decoder->image->icc.size) {
    iccProfile.resize(decoder->image->icc.size);
    std::copy(decoder->image->icc.data,
              decoder->image->icc.data + decoder->image->icc.size,
              iccProfile.begin());
  }

  uint32_t imageWidth = decoder->image->width;
  uint32_t imageHeight = decoder->image->height;

  uint32_t stride = avifUniqueImage.rgbImage.rowBytes;

  aligned_uint8_vector imageStore;

  imageStore = RescaleSourceImage(avifUniqueImage.rgbImage.pixels, &stride,
                                  bitDepth, isImageRequires64Bit, &imageWidth,
                                  &imageHeight, scaledWidth, scaledHeight, javaScaleMode,
                                  scalingQuality, imageUsesAlpha);

  avifUniqueImage.clear();

  std::optional<GainmapFrame> gainMap;
  const auto gain = decoder->image->gainMap;
  if (androidOSVersion() >= 34 && gain && gain->image && gain->image->yuvPlanes[0]) {
    auto fraction = [](auto f) -> float {
      if (!f.d) throw std::runtime_error("Invalid gainmap fraction");
      return static_cast<float>(f.n) / static_cast<float>(f.d);
    };
    GainmapFrame decodedGain{};
    for (int c = 0; c < 3; ++c) {
      decodedGain.metadata[c] = fraction(gain->gainMapMin[c]);
      decodedGain.metadata[3 + c] = fraction(gain->gainMapMax[c]);
      decodedGain.metadata[6 + c] = fraction(gain->gainMapGamma[c]);
      decodedGain.metadata[9 + c] = fraction(gain->baseOffset[c]);
      decodedGain.metadata[12 + c] = fraction(gain->alternateOffset[c]);
    }
    decodedGain.metadata[15] = fraction(gain->baseHdrHeadroom);
    decodedGain.metadata[16] = fraction(gain->alternateHdrHeadroom);
    decodedGain.metadata[17] = gain->useBaseColorSpace ? 1.f : 0.f;
    // Android data spaces for the supported alternate primaries.
    decodedGain.metadata[18] = -1.f;
    if (gain->altColorPrimaries == AVIF_COLOR_PRIMARIES_BT709)
      decodedGain.metadata[18] = 0x08810000;
    else if (gain->altColorPrimaries == AVIF_COLOR_PRIMARIES_SMPTE432)
      decodedGain.metadata[18] = 0x088A0000;
    else if (gain->altColorPrimaries == AVIF_COLOR_PRIMARIES_BT2020)
      decodedGain.metadata[18] = 0x08C60000;
    const bool hdrBase = decodedGain.metadata[15] > decodedGain.metadata[16];
    if (!hdrBase || androidOSVersion() >= 36) {
      avifRGBImage rgb{};
      avifRGBImageSetDefaults(&rgb, gain->image);
      rgb.format = AVIF_RGB_FORMAT_RGBA;
      rgb.depth = gain->image->depth;
      // Bitmap storage must have opaque alpha. ignoreAlpha leaves that byte
      // unwritten, causing Android to unpremultiply arbitrary alpha values.
      rgb.ignoreAlpha = AVIF_FALSE;
      if (avifRGBImageAllocatePixels(&rgb) != AVIF_RESULT_OK)
        throw std::runtime_error("Cannot allocate gainmap pixels");
      struct PixelsGuard { avifRGBImage *rgb; ~PixelsGuard() { avifRGBImageFreePixels(rgb); }}
          pixelsGuard{&rgb};
      if (avifImageYUVToRGB(gain->image, &rgb) != AVIF_RESULT_OK)
        throw std::runtime_error("Cannot decode gainmap pixels");
      decodedGain.width = rgb.width;
      decodedGain.height = rgb.height;
      decodedGain.bitDepth = rgb.depth;
      auto gainStride = rgb.rowBytes;
      decodedGain.store = RescaleSourceImage(rgb.pixels,
                                             &gainStride,
                                             rgb.depth,
                                             rgb.depth > 8,
                                             &decodedGain.width,
                                             &decodedGain.height,
                                             scaledWidth,
                                             scaledHeight,
                                             javaScaleMode,
                                             scalingQuality,
                                             false);
      gainMap = std::move(decodedGain);
    }
  }

  // Keep the base rendition in its original color space: converting it alone
  // would change the meaning of per-channel gains and offsets.
  if (!gainMap && !iccProfile.empty()) {
    convertUseICC(imageStore, stride, imageWidth, imageHeight, iccProfile.data(),
                  iccProfile.size(),
                  isImageRequires64Bit, bitDepth);
  } else if (!gainMap && (transferCharacteristics != AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED
      || colorPrimaries != AVIF_COLOR_PRIMARIES_UNSPECIFIED)) {
    Eigen::Matrix<float, 3, 2> primaries;
    float imagePrimaries[8] = {0.64f, 0.33f, 0.3f, 0.6f, 0.15f, 0.06f, 0.3127f, 0.329f};
    avifColorPrimariesGetValues(colorPrimaries, imagePrimaries);

    primaries << static_cast<float>(imagePrimaries[0]),
        static_cast<float>(imagePrimaries[1]),
        static_cast<float>(imagePrimaries[2]),
        static_cast<float>(imagePrimaries[3]),
        static_cast<float>(imagePrimaries[4]),
        static_cast<float>(imagePrimaries[5]);

    Eigen::Vector2f whitePoint;
    whitePoint << static_cast<float>(imagePrimaries[6]),
        static_cast<float>(imagePrimaries[7]);

    Eigen::Matrix3f destinationProfile = GamutRgbToXYZ(getSRGBPrimaries(), getIlluminantD65());
    Eigen::Matrix3f sourceProfile = GamutRgbToXYZ(primaries, whitePoint);

    Eigen::Matrix3f conversion = destinationProfile.inverse() * sourceProfile;

    ToneMapping toneMapping = ToneMapping::Rec2408;

    if (transferCharacteristics !=
        AVIF_TRANSFER_CHARACTERISTICS_HLG &&
        transferCharacteristics != AVIF_TRANSFER_CHARACTERISTICS_PQ) {
      toneMapping = ToneMapping::Skip;
    }

    FfiTrc transferFfi = FfiTrc::Srgb;

    if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_HLG) {
      transferFfi = FfiTrc::Hlg;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_SMPTE428) {
      transferFfi = FfiTrc::Smpte428;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_PQ) {
      transferFfi = FfiTrc::Smpte2084;
    } else if (transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_LINEAR) {
      transferFfi = FfiTrc::Linear;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_BT470M) {
      transferFfi = FfiTrc::Bt470M;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_BT470BG) {
      transferFfi = FfiTrc::Bt470Bg;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_BT601) {
      transferFfi = FfiTrc::Bt709;
    } else if (transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_BT709) {
      transferFfi = FfiTrc::Bt709;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_BT2020_10BIT ||
        transferCharacteristics ==
            AVIF_TRANSFER_CHARACTERISTICS_BT2020_12BIT) {
      transferFfi = FfiTrc::Bt709;
    } else if (transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_SMPTE240) {
      transferFfi = FfiTrc::Smpte240;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_LOG100) {
      transferFfi = FfiTrc::Log100;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_LOG100_SQRT10) {
      transferFfi = FfiTrc::Log100sqrt10;
    } else if (transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_SRGB) {
      transferFfi = FfiTrc::Srgb;
    } else if (transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_IEC61966) {
      transferFfi = FfiTrc::Iec61966;
    } else if (transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_BT1361) {
      transferFfi = FfiTrc::Bt1361;
    } else if (transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED) {
      transferFfi = FfiTrc::Srgb;
    }

    const float cPrimaries[6] = {
        imagePrimaries[0], imagePrimaries[1],
        imagePrimaries[2], imagePrimaries[3],
        imagePrimaries[4], imagePrimaries[5]
    };
    const float wp[2] = {
        whitePoint(0), whitePoint(1)
    };

    if (isImageRequires64Bit) {
      apply_tone_mapping_rgba16(
          reinterpret_cast<uint16_t *>(imageStore.data()), stride, bitDepth,
          imageWidth, imageHeight, cPrimaries, wp, transferFfi, toneMapping, intensityTarget
      );
    } else {
      apply_tone_mapping_rgba8(
          reinterpret_cast<uint8_t *>(imageStore.data()), stride,
          imageWidth, imageHeight, cPrimaries, wp, transferFfi, toneMapping, intensityTarget
      );
    }

  }

  AvifImageFrame imageFrame = {
      .store = imageStore,
      .width = imageWidth,
      .height = imageHeight,
      .is16Bit = isImageRequires64Bit,
      .bitDepth = bitDepth,
      .hasAlpha = imageUsesAlpha,
      .gainMap = std::move(gainMap),
      .colorPrimaries = colorPrimaries,
      .transferCharacteristics = transferCharacteristics
  };
  return imageFrame;
}

void AvifDecoderController::attachBuffer(uint8_t *data, uint32_t bufferSize) {
  std::lock_guard guard(this->mutex);
  if (this->isBufferAttached) {
    throw std::runtime_error("AVIF controller can accept buffer only once");
  }
  this->buffer.resize(bufferSize);
  std::copy(data, data + bufferSize, this->buffer.begin());
  auto result =
      avifDecoderSetIOMemory(this->decoder.get(), this->buffer.data(), this->buffer.size());
  if (result != AVIF_RESULT_OK) {
    throw std::runtime_error("Can't successfully attach memory");
  }
  this->decoder->ignoreExif = false;
  this->decoder->ignoreXMP = false;
  this->decoder->strictFlags = AVIF_STRICT_DISABLED;
  this->decoder->imageContentToDecode =
      androidOSVersion() >= 34 ? AVIF_IMAGE_CONTENT_ALL : AVIF_IMAGE_CONTENT_COLOR_AND_ALPHA;

  uint32_t hwThreads = std::thread::hardware_concurrency();
  this->decoder->maxThreads = static_cast<int>(hwThreads);
  result = avifDecoderParse(decoder.get());
  if (result != AVIF_RESULT_OK) {
    throw std::runtime_error("This is doesn't looks like AVIF image");
  }
  this->isBufferAttached = true;
}

uint32_t AvifDecoderController::getFramesCount() {
  std::lock_guard guard(this->mutex);
  if (!this->isBufferAttached) {
    throw std::runtime_error("AVIF controller methods can't be called without attached buffer");
  }
  return this->decoder->imageCount;
}

uint32_t AvifDecoderController::getLoopsCount() {
  std::lock_guard guard(this->mutex);
  if (!this->isBufferAttached) {
    throw std::runtime_error("AVIF controller methods can't be called without attached buffer");
  }
  return this->decoder->repetitionCount;
}

uint32_t AvifDecoderController::getFrameDuration(uint32_t frame) {
  std::lock_guard guard(this->mutex);
  if (!this->isBufferAttached) {
    throw std::runtime_error("AVIF controller methods can't be called without attached buffer");
  }

  if (frame >= this->decoder->imageCount) {
    std::string str = "Can't time of frame number: " + std::to_string(frame);
    throw std::runtime_error(str);
  }

  avifImageTiming timing;
  auto result = avifDecoderNthImageTiming(this->decoder.get(), frame, &timing);
  if (result != AVIF_RESULT_OK) {
    std::string str = "Can't time of frame number: " + std::to_string(frame);
    throw std::runtime_error(str);
  }
  return (uint32_t) (1000.0f / ((float) timing.timescale) * (float) timing.durationInTimescales);
}

uint32_t AvifDecoderController::getTotalDuration() {
  std::lock_guard guard(this->mutex);
  if (!this->isBufferAttached) {
    throw std::runtime_error("AVIF controller methods can't be called without attached buffer");
  }
  return (uint32_t) (1000.0f / ((float) this->decoder->timescale)
      * (float) this->decoder->durationInTimescales);
}

AvifImageSize AvifDecoderController::getImageSize() {
  std::lock_guard guard(this->mutex);
  if (!this->isBufferAttached) {
    throw std::runtime_error("AVIF controller methods can't be called without attached buffer");
  }
  if (!this->decoder->image) {
    throw std::runtime_error("Parsed image is expected but there are nothing");
  }
  AvifImageSize imageSize = {
      .width = this->decoder->image->width,
      .height = this->decoder->image->height,
  };
  return imageSize;
}

AvifImageSize AvifDecoderController::getImageSize(uint8_t *data, uint32_t bufferSize) {
  auto decoder = avif::DecoderPtr(avifDecoderCreate());
  if (!decoder) {
    throw std::runtime_error("Can't create decoder");
  }
  auto result =
      avifDecoderSetIOMemory(decoder.get(), data, bufferSize);
  if (result != AVIF_RESULT_OK) {
    throw std::runtime_error("Can't successfully attach memory");
  }
  decoder->ignoreExif = false;
  decoder->ignoreXMP = false;
  decoder->strictFlags = AVIF_STRICT_DISABLED;

  result = avifDecoderParse(decoder.get());
  if (result != AVIF_RESULT_OK) {
    throw std::runtime_error("This is doesn't looks like AVIF image");
  }
  if (!decoder->image) {
    throw std::runtime_error("Image is expected but after decoding there are none");
  }
  AvifImageSize size = {
      .width = decoder->image->width,
      .height = decoder->image->height
  };
  return size;
}
