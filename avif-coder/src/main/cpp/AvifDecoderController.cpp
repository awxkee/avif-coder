//
// Created by Radzivon Bartoshyk on 12/10/2024.
//

#include "AvifDecoderController.h"
#include "avif/avif.h"
#include <exception>
#include <thread>
#include "imagebits/CopyUnalignedRGBA.h"
#include "colorspace.h"
#include <Trc.h>
#include "Eigen/Eigen"
#include "ColorSpaceProfile.h"
#include <ITUR.h>
#include "ColorMatrix.h"

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
    if (result != AVIF_RESULT_OK) {
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
                                               uint32_t scaledWidth,
                                               uint32_t scaledHeight,
                                               PreferredColorConfig javaColorSpace,
                                               ScaleMode javaScaleMode,
                                               CurveToneMapper toneMapper,
                                               bool highQualityResizer) {
  std::lock_guard guard(this->mutex);
  if (this->isBufferAttached) {
    throw std::runtime_error("AVIF controller can accept buffer only once");
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

  bool isImageRequires64Bit = avifImageUsesU16(decoder->image);
  if (isImageRequires64Bit) {
    avifUniqueImage.rgbImage.alphaPremultiplied = false;
    avifUniqueImage.rgbImage.depth = 16;
  } else {
    avifUniqueImage.rgbImage.alphaPremultiplied = false;
    avifUniqueImage.rgbImage.depth = 8;
  }

  uint32_t bitDepth = avifUniqueImage.rgbImage.depth;

  avifResult rgbResult = avifUniqueImage.allocateImage();
  if (rgbResult != AVIF_RESULT_OK) {
    std::string
        str = "Can't correctly allocate buffer for frame with numbers: " + std::to_string(frame);
    throw std::runtime_error(str);
  }
  rgbResult = avifImageYUVToRGB(decoder->image, &avifUniqueImage.rgbImage);
  if (rgbResult != AVIF_RESULT_OK) {
    std::string
        str = "Can't correctly allocate buffer for frame with numbers: " + std::to_string(frame);
    throw std::runtime_error(str);
  }

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
                                  highQualityResizer);

  avifUniqueImage.clear();

  if (!iccProfile.empty()) {
    convertUseICC(imageStore, stride, imageWidth, imageHeight, iccProfile.data(),
                  iccProfile.size(),
                  isImageRequires64Bit);
  } else if (transferCharacteristics != AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED
      || colorPrimaries != AVIF_COLOR_PRIMARIES_UNSPECIFIED) {
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

    if (transferCharacteristics !=
        AVIF_TRANSFER_CHARACTERISTICS_HLG &&
        transferCharacteristics != AVIF_TRANSFER_CHARACTERISTICS_PQ) {
      toneMapper = TONE_SKIP;
    }

    TransferFunction forwardTrc = TransferFunction::Srgb;

    if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_HLG) {
      forwardTrc = TransferFunction::Hlg;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_SMPTE428) {
      forwardTrc = TransferFunction::Smpte428;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_PQ) {
      forwardTrc = TransferFunction::Pq;
    } else if (transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_LINEAR) {
      forwardTrc = TransferFunction::Linear;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_BT470M) {
      forwardTrc = TransferFunction::Gamma2p2;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_BT470BG) {
      forwardTrc = TransferFunction::Gamma2p8;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_BT601) {
      forwardTrc = TransferFunction::Itur709;
    } else if (transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_BT709) {
      forwardTrc = TransferFunction::Itur709;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_BT2020_10BIT ||
        transferCharacteristics ==
            AVIF_TRANSFER_CHARACTERISTICS_BT2020_12BIT) {
      forwardTrc = TransferFunction::Itur709;
    } else if (transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_SMPTE240) {
      forwardTrc = TransferFunction::Smpte240;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_LOG100) {
      forwardTrc = TransferFunction::Log100;
    } else if (transferCharacteristics ==
        AVIF_TRANSFER_CHARACTERISTICS_LOG100_SQRT10) {
      forwardTrc = TransferFunction::Log100Sqrt10;
    } else if (transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_SRGB) {
      forwardTrc = TransferFunction::Srgb;
    } else if (transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_IEC61966) {
      forwardTrc = TransferFunction::Iec61966;
    } else if (transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_BT1361) {
      forwardTrc = TransferFunction::Bt1361;
    } else if (transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED) {
      forwardTrc = TransferFunction::Srgb;
    }

    ITURColorCoefficients coeffs = colorPrimariesComputeYCoeffs(primaries, whitePoint);

    const float matrix[9] = {
        conversion(0, 0), conversion(0, 1), conversion(0, 2),
        conversion(1, 0), conversion(1, 1), conversion(1, 2),
        conversion(2, 0), conversion(2, 1), conversion(2, 2),
    };

    if (isImageRequires64Bit) {
      applyColorMatrix16Bit(reinterpret_cast<uint16_t *>(imageStore.data()),
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
      applyColorMatrix(reinterpret_cast<uint8_t *>(imageStore.data()), stride, imageWidth,
                       imageHeight, matrix, forwardTrc, TransferFunction::Srgb, toneMapper,
                       coeffs);
    }

  }

  AvifImageFrame imageFrame = {
      .store = imageStore,
      .width = imageWidth,
      .height = imageHeight,
      .is16Bit = isImageRequires64Bit,
      .bitDepth = bitDepth,
      .hasAlpha = imageUsesAlpha
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

  uint32_t hwThreads = std::thread::hardware_concurrency();
  this->decoder->maxThreads = static_cast<int>(hwThreads);
  result = avifDecoderParse(decoder.get());
  if (result != AVIF_RESULT_OK) {
    throw std::runtime_error("This is doesn't looks like AVIF image");
  }
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
  AvifImageSize imageSize = {
      .width = this->decoder->image->width,
      .height = this->decoder->image->height,
  };
  return imageSize;
}