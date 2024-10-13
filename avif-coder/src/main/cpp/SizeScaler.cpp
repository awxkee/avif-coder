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

#include "SizeScaler.h"
#include <vector>
#include "imagebits/CopyUnalignedRGBA.h"
#include "heif.h"
#include <string>
#include <jni.h>
#include "JniException.h"
#include "definitions.h"
#include "avifweaver.h"

bool RescaleImage(aligned_uint8_vector &initialData,
                  std::shared_ptr<heif_image_handle> &handle,
                  std::shared_ptr<heif_image> &img,
                  int *stride,
                  bool useFloats,
                  int *imageWidthPtr, int *imageHeightPtr,
                  int scaledWidth, int scaledHeight, ScaleMode scaleMode, bool highQualityScaling) {
  int imageWidth = *imageWidthPtr;
  int imageHeight = *imageHeightPtr;
  if ((scaledHeight != 0 || scaledWidth != 0) && (scaledWidth != 0 && scaledHeight != 0)) {

    int xTranslation = 0, yTranslation = 0;
    int canvasWidth = scaledWidth;
    int canvasHeight = scaledHeight;

    if (scaleMode == Fit || scaleMode == Fill) {
      std::pair<uint32_t, uint32_t> currentSize(imageWidth, imageHeight);
      if (scaledHeight > 0 && scaledWidth < 0) {
        auto newBounds = ResizeAspectHeight(currentSize, scaledHeight,
                                            scaledWidth == -2);
        scaledWidth = static_cast<int>(newBounds.first);
        scaledHeight = static_cast<int>(newBounds.second);
      } else if (scaledHeight < 0) {
        auto newBounds = ResizeAspectWidth(currentSize, scaledHeight,
                                           scaledHeight == -2);
        scaledWidth = static_cast<int>(newBounds.first);
        scaledHeight = static_cast<int>(newBounds.second);
      } else {
        std::pair<uint32_t, uint32_t> dstSize;
        float scale = 1;
        if (scaleMode == Fill) {
          std::pair<uint32_t, uint32_t> canvasSize(scaledWidth, scaledHeight);
          dstSize = ResizeAspectFill(currentSize, canvasSize, &scale);
        } else {
          std::pair<uint32_t, uint32_t> canvasSize(scaledWidth, scaledHeight);
          dstSize = ResizeAspectFit(currentSize, canvasSize, &scale);
        }

        xTranslation = std::max((int) (((float) dstSize.first - (float) canvasWidth) /
            2.0f), 0);
        yTranslation = std::max((int) (((float) dstSize.second - (float) canvasHeight) /
            2.0f), 0);

        scaledWidth = static_cast<int>(dstSize.first);
        scaledHeight = static_cast<int>(dstSize.second);
      }
    }

    int outStride;
    auto
        imagePlane = heif_image_get_plane_readonly(img.get(), heif_channel_interleaved, &outStride);
    if (imagePlane == nullptr) {
      std::string exception = "Can't get an image plane";
      throw std::runtime_error(exception);
    }

    aligned_uint8_vector outData;

    auto bitDepth = heif_image_handle_get_chroma_bits_per_pixel(handle.get());
    if (bitDepth == 8) {
      outData.resize(scaledHeight * scaledWidth * 4);
      weave_scale_u8(imagePlane,
                     outStride,
                     imageWidth,
                     imageHeight,
                     outData.data(),
                     scaledWidth * 4,
                     scaledWidth,
                     scaledHeight,
                     highQualityScaling);
    } else {
      outData.resize(scaledHeight * scaledWidth * 4 * sizeof(uint16_t));
      weave_scale_u16(reinterpret_cast<const uint16_t *>(imagePlane),
                      outStride,
                      imageWidth,
                      imageHeight,
                      reinterpret_cast<uint16_t *>(outData.data()),
                      scaledWidth,
                      scaledHeight,
                      bitDepth,
                      highQualityScaling);
    }

    auto data = outData.data();

    *stride = static_cast<int >(scaledWidth) * 4
        * static_cast<int>(useFloats ? sizeof(uint16_t) : sizeof(uint8_t));

    imageWidth = scaledWidth;
    imageHeight = scaledHeight;

    if (xTranslation > 0 || yTranslation > 0) {

      int left = std::max(xTranslation, 0);
      int right = xTranslation + canvasWidth;
      int top = std::max(yTranslation, 0);
      int bottom = yTranslation + canvasHeight;

      int croppedWidth = right - left;
      int croppedHeight = bottom - top;
      int newStride =
          croppedWidth * 4 * (int) (useFloats ? sizeof(uint16_t) : sizeof(uint8_t));
      int srcStride = *stride;

      aligned_uint8_vector croppedImage(newStride * croppedHeight);

      uint8_t *dstData = croppedImage.data();
      auto srcData = reinterpret_cast<const uint8_t *>(data);

      for (int y = top, yc = 0; y < bottom; ++y, ++yc) {
        int x = 0;
        int xc = 0;
        auto srcRow = reinterpret_cast<const uint8_t *>(srcData + srcStride * y);
        auto dstRow = reinterpret_cast<uint8_t *>(dstData + newStride * yc);
        for (x = left, xc = 0; x < right; ++x, ++xc) {
          if (useFloats) {
            auto dst = reinterpret_cast<uint64_t *>(dstRow);
            auto src = reinterpret_cast<const uint64_t *>(srcRow);
            dst[xc] = src[x];
          } else {
            auto dst = reinterpret_cast<uint32_t *>(dstRow);
            auto src = reinterpret_cast<const uint32_t *>(srcRow);
            dst[xc] = src[x];
          }
        }
      }

      imageWidth = croppedWidth;
      imageHeight = croppedHeight;

      initialData = croppedImage;
      *stride = newStride;

    } else {
      initialData.resize(*stride * imageHeight);
      if (useFloats) {
        coder::CopyUnaligned(reinterpret_cast<const uint16_t *>(data), *stride,
                             reinterpret_cast<uint16_t *>(initialData.data()), *stride,
                             imageWidth * 4,
                             imageHeight);
      } else {
        coder::CopyUnaligned(reinterpret_cast<const uint8_t *>(data), *stride,
                             reinterpret_cast<uint8_t *>(initialData.data()), *stride,
                             imageWidth * 4,
                             imageHeight);
      }
    }

    *imageWidthPtr = imageWidth;
    *imageHeightPtr = imageHeight;
  } else {
    auto data = heif_image_get_plane_readonly(img.get(), heif_channel_interleaved, stride);
    if (!data) {
      std::string exception = "Interleaving planed has failed";
      throw std::runtime_error(exception);
    }

    imageWidth = heif_image_get_width(img.get(), heif_channel_interleaved);
    imageHeight = heif_image_get_height(img.get(), heif_channel_interleaved);
    initialData.resize(*stride * imageHeight);

    if (useFloats) {
      coder::CopyUnaligned(reinterpret_cast<const uint16_t *>(data), *stride,
                           reinterpret_cast<uint16_t *>(initialData.data()), *stride,
                           imageWidth * 4,
                           imageHeight);
    } else {
      coder::CopyUnaligned(reinterpret_cast<const uint8_t *>(data), *stride,
                           reinterpret_cast<uint8_t *>(initialData.data()), *stride,
                           imageWidth * 4,
                           imageHeight);
    }

    *imageWidthPtr = imageWidth;
    *imageHeightPtr = imageHeight;
  }
  return true;
}

aligned_uint8_vector RescaleSourceImage(uint8_t *sourceData,
                                        uint32_t *stride,
                                        uint32_t bitDepth,
                                        bool isImage64Bits,
                                        uint32_t *imageWidthPtr,
                                        uint32_t *imageHeightPtr,
                                        uint32_t scaledWidth,
                                        uint32_t scaledHeight,
                                        ScaleMode scaleMode,
                                        bool highQualityScaling) {
  uint32_t imageWidth = *imageWidthPtr;
  uint32_t imageHeight = *imageHeightPtr;
  if ((scaledHeight != 0 || scaledWidth != 0) && (scaledWidth != 0 && scaledHeight != 0)) {

    int xTranslation = 0, yTranslation = 0;
    uint32_t canvasWidth = scaledWidth;
    uint32_t canvasHeight = scaledHeight;

    if (scaleMode == Fit || scaleMode == Fill) {
      std::pair<uint32_t, uint32_t> currentSize(imageWidth, imageHeight);
      if (scaledHeight > 0 && scaledWidth < 0) {
        auto newBounds = ResizeAspectHeight(currentSize, scaledHeight,
                                            scaledWidth == -2);
        scaledWidth = newBounds.first;
        scaledHeight = newBounds.second;
      } else if (scaledHeight < 0) {
        auto newBounds = ResizeAspectWidth(currentSize, scaledHeight,
                                           scaledHeight == -2);
        scaledWidth = newBounds.first;
        scaledHeight = newBounds.second;
      } else {
        std::pair<uint32_t, uint32_t> dstSize;
        float scale = 1;
        if (scaleMode == Fill) {
          std::pair<uint32_t, uint32_t> canvasSize(scaledWidth, scaledHeight);
          dstSize = ResizeAspectFill(currentSize, canvasSize, &scale);
        } else {
          std::pair<uint32_t, uint32_t> canvasSize(scaledWidth, scaledHeight);
          dstSize = ResizeAspectFit(currentSize, canvasSize, &scale);
        }

        xTranslation = std::max((int) (((float) dstSize.first - (float) canvasWidth) /
            2.0f), 0);
        yTranslation = std::max((int) (((float) dstSize.second - (float) canvasHeight) /
            2.0f), 0);

        scaledWidth = dstSize.first;
        scaledHeight = dstSize.second;
      }
    }

    aligned_uint8_vector outData;

    if (bitDepth == 8) {
      outData.resize(scaledHeight * scaledWidth * 4);
      weave_scale_u8(sourceData,
                     *stride,
                     imageWidth,
                     imageHeight,
                     outData.data(),
                     scaledWidth * 4,
                     scaledWidth,
                     scaledHeight,
                     highQualityScaling);
    } else {
      outData.resize(scaledHeight * scaledWidth * 4 * sizeof(uint16_t));
      weave_scale_u16(reinterpret_cast<const uint16_t *>(sourceData),
                      *stride,
                      imageWidth,
                      imageHeight,
                      reinterpret_cast<uint16_t *>(outData.data()),
                      scaledWidth,
                      scaledHeight,
                      bitDepth,
                      highQualityScaling);
    }

    auto data = outData.data();

    *stride = scaledWidth * 4 * (isImage64Bits ? sizeof(uint16_t) : sizeof(uint8_t));

    imageWidth = scaledWidth;
    imageHeight = scaledHeight;

    if (xTranslation > 0 || yTranslation > 0) {

      int left = std::max(xTranslation, 0);
      int right = xTranslation + static_cast<int>(canvasWidth);
      int top = std::max(yTranslation, 0);
      int bottom = yTranslation + static_cast<int>(canvasHeight);

      int croppedWidth = right - left;
      int croppedHeight = bottom - top;
      uint32_t newStride =
          croppedWidth * 4 * (int) (isImage64Bits ? sizeof(uint16_t) : sizeof(uint8_t));
      uint32_t srcStride = *stride;

      aligned_uint8_vector croppedImage(newStride * croppedHeight);

      uint8_t *dstData = croppedImage.data();
      auto srcData = reinterpret_cast<const uint8_t *>(data);

      for (int y = top, yc = 0; y < bottom; ++y, ++yc) {
        int x = 0;
        int xc = 0;
        auto srcRow = reinterpret_cast<const uint8_t *>(srcData + srcStride * y);
        auto dstRow = reinterpret_cast<uint8_t *>(dstData + newStride * yc);
        for (x = left, xc = 0; x < right; ++x, ++xc) {
          if (isImage64Bits) {
            auto dst = reinterpret_cast<uint64_t *>(dstRow);
            auto src = reinterpret_cast<const uint64_t *>(srcRow);
            dst[xc] = src[x];
          } else {
            auto dst = reinterpret_cast<uint32_t *>(dstRow);
            auto src = reinterpret_cast<const uint32_t *>(srcRow);
            dst[xc] = src[x];
          }
        }
      }

      imageWidth = croppedWidth;
      imageHeight = croppedHeight;

      *imageWidthPtr = imageWidth;
      *imageHeightPtr = imageHeight;

      *stride = newStride;
      return croppedImage;
    } else {
      *imageWidthPtr = imageWidth;
      *imageHeightPtr = imageHeight;
      return outData;
    }
  } else {
    uint32_t newStride = imageWidth * 4 * (isImage64Bits ? sizeof(uint16_t) : sizeof(uint8_t));
    aligned_uint8_vector dataStore(newStride * imageHeight);

    if (isImage64Bits) {
      coder::CopyUnaligned(reinterpret_cast<const uint16_t *>(sourceData), *stride,
                           reinterpret_cast<uint16_t *>(dataStore.data()), newStride,
                           imageWidth * 4,
                           imageHeight);
    } else {
      coder::CopyUnaligned(reinterpret_cast<const uint8_t *>(sourceData), *stride,
                           reinterpret_cast<uint8_t *>(dataStore.data()), newStride,
                           imageWidth * 4,
                           imageHeight);
    }

    *imageWidthPtr = imageWidth;
    *imageHeightPtr = imageHeight;
    return dataStore;
  }
}

std::pair<uint32_t, uint32_t>
ResizeAspectFit(std::pair<uint32_t, uint32_t> sourceSize,
                std::pair<uint32_t, uint32_t> dstSize,
                float *scale) {
  uint32_t sourceWidth = sourceSize.first;
  uint32_t sourceHeight = sourceSize.second;
  float xFactor = (float) dstSize.first / (float) sourceSize.first;
  float yFactor = (float) dstSize.second / (float) sourceSize.second;
  float resizeFactor = std::min(xFactor, yFactor);
  *scale = resizeFactor;
  std::pair<uint32_t, uint32_t> resultSize((uint32_t) ((float) sourceWidth * resizeFactor),
                                           (uint32_t) ((float) sourceHeight * resizeFactor));
  return resultSize;
}

std::pair<uint32_t, uint32_t>
ResizeAspectFill(std::pair<uint32_t, uint32_t> sourceSize,
                 std::pair<uint32_t, uint32_t> dstSize,
                 float *scale) {
  uint32_t sourceWidth = sourceSize.first;
  uint32_t sourceHeight = sourceSize.second;
  float xFactor = (float) dstSize.first / (float) sourceSize.first;
  float yFactor = (float) dstSize.second / (float) sourceSize.second;
  float resizeFactor = std::max(xFactor, yFactor);
  *scale = resizeFactor;
  std::pair<uint32_t, uint32_t> resultSize((uint32_t) ((float) sourceWidth * resizeFactor),
                                           (uint32_t) ((float) sourceHeight * resizeFactor));
  return resultSize;
}

std::pair<uint32_t, uint32_t>
ResizeAspectHeight(std::pair<uint32_t, uint32_t> sourceSize, uint32_t maxHeight, bool multipleBy2) {
  uint32_t sourceWidth = sourceSize.first;
  uint32_t sourceHeight = sourceSize.second;
  float scaleFactor = (float) maxHeight / (float) sourceSize.second;
  std::pair<uint32_t, uint32_t> resultSize((uint32_t) ((float) sourceWidth * scaleFactor),
                                           (uint32_t) ((float) sourceHeight * scaleFactor));
  if (multipleBy2) {
    resultSize.first = (resultSize.first / 2) * 2;
    resultSize.second = (resultSize.second / 2) * 2;
  }
  return resultSize;
}

std::pair<uint32_t, uint32_t>
ResizeAspectWidth(std::pair<uint32_t, uint32_t> sourceSize, uint32_t maxWidth, bool multipleBy2) {
  uint32_t sourceWidth = sourceSize.first;
  uint32_t sourceHeight = sourceSize.second;
  float scaleFactor = (float) maxWidth / (float) sourceSize.first;
  std::pair<uint32_t, uint32_t> resultSize((uint32_t) ((float) sourceWidth * scaleFactor),
                                           (uint32_t) ((float) sourceHeight * scaleFactor));
  if (multipleBy2) {
    resultSize.first = (resultSize.first / 2) * 2;
    resultSize.second = (resultSize.second / 2) * 2;
  }
  return resultSize;
}