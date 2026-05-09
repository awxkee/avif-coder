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
                  int scaledWidth, int scaledHeight, ScaleMode scaleMode, int scalingQuality,
                  bool isRgba) {
  int imageWidth = *imageWidthPtr;
  int imageHeight = *imageHeightPtr;
  if ((scaledHeight != 0 || scaledWidth != 0) && (scaledWidth != 0 && scaledHeight != 0)) {
    int outStride;
    auto
        imagePlane = heif_image_get_plane_readonly(img.get(), heif_channel_interleaved, &outStride);
    if (imagePlane == nullptr) {
      std::string exception = "Can't get an image plane";
      throw std::runtime_error(exception);
    }

    WeaveScaleMode mScaleMode = WeaveScaleMode::JustResize;
    switch (scaleMode) {
      case Fit:mScaleMode = WeaveScaleMode::ScaleToFit;
        break;
      case Fill:mScaleMode = WeaveScaleMode::ScaleToFill;
        break;
      case Resize:mScaleMode = WeaveScaleMode::JustResize;
        break;
    }

    aligned_uint8_vector outData;

    auto bitDepth = heif_image_handle_get_chroma_bits_per_pixel(handle.get());
    if (bitDepth == 8) {
      auto scalingResult = weave_scale_u8(imagePlane,
                                          outStride,
                                          imageWidth,
                                          imageHeight,
                                          scaledWidth,
                                          scaledHeight,
                                          scalingQuality,
                                          isRgba,
                                          mScaleMode
      );
      if (scalingResult.data == nullptr) {
        std::string exception = "Scaling image has failed";
        throw std::runtime_error(exception);
      }
      initialData.resize(scalingResult.length);
      *imageWidthPtr = static_cast<int>(scalingResult.width);
      *imageHeightPtr = static_cast<int>(scalingResult.height);
      *stride = static_cast<int>(scalingResult.stride);
      memcpy(
          initialData.data(),
          scalingResult.data,
          scalingResult.length
      );
      weave_scaling_result_free(scalingResult);
      return true;
    } else {
      auto scalingResult = weave_scale_u16(reinterpret_cast<const uint16_t *>(imagePlane),
                                           outStride,
                                           imageWidth,
                                           imageHeight,
                                           scaledWidth,
                                           scaledHeight,
                                           bitDepth,
                                           scalingQuality,
                                           isRgba,
                                           mScaleMode
      );
      if (scalingResult.data == nullptr) {
        std::string exception = "Scaling image has failed";
        throw std::runtime_error(exception);
      }
      initialData.resize(scalingResult.length * sizeof(uint16_t));
      *imageWidthPtr = static_cast<int>(scalingResult.width);
      *imageHeightPtr = static_cast<int>(scalingResult.height);
      *stride = static_cast<int>(scalingResult.stride * 2);
      memcpy(
          initialData.data(),
          reinterpret_cast<const uint8_t * >(scalingResult.data),
          scalingResult.length * sizeof(uint16_t)
      );
      weave_scaling_result16_free(scalingResult);
      return true;
    }
  } else {
    auto data = heif_image_get_plane_readonly(img.get(), heif_channel_interleaved, stride);
    if (!data) {
      std::string exception = "Interleaving planed has failed";
      throw std::runtime_error(exception);
    }

    imageWidth = heif_image_get_width(img.get(), heif_channel_interleaved);
    imageHeight = heif_image_get_height(img.get(), heif_channel_interleaved);

    uint32_t newStride = static_cast<int >(imageWidth) * 4
        * static_cast<int>(useFloats ? sizeof(uint16_t) : sizeof(uint8_t));

    initialData.resize(newStride * imageHeight);

    if (useFloats) {
      coder::CopyUnaligned(reinterpret_cast<const uint16_t *>(data), *stride,
                           reinterpret_cast<uint16_t *>(initialData.data()), newStride,
                           imageWidth * 4,
                           imageHeight);
    } else {
      coder::CopyUnaligned(reinterpret_cast<const uint8_t *>(data), *stride,
                           reinterpret_cast<uint8_t *>(initialData.data()), newStride,
                           imageWidth * 4,
                           imageHeight);
    }

    *stride = static_cast<int>(newStride);

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
                                        int32_t scaledWidth,
                                        int32_t scaledHeight,
                                        ScaleMode scaleMode,
                                        int scalingQuality,
                                        bool isRgba) {
  uint32_t imageWidth = *imageWidthPtr;
  uint32_t imageHeight = *imageHeightPtr;
  if ((scaledHeight != 0 || scaledWidth != 0) && (scaledWidth != 0 && scaledHeight != 0)) {
    WeaveScaleMode mScaleMode = WeaveScaleMode::JustResize;
    switch (scaleMode) {
      case Fit:mScaleMode = WeaveScaleMode::ScaleToFit;
        break;
      case Fill:mScaleMode = WeaveScaleMode::ScaleToFill;
        break;
      case Resize:mScaleMode = WeaveScaleMode::JustResize;
        break;
    }

    aligned_uint8_vector outData;

    if (bitDepth == 8) {
      auto scalingResult = weave_scale_u8(sourceData,
                                          *stride,
                                          imageWidth,
                                          imageHeight,
                                          scaledWidth,
                                          scaledHeight,
                                          scalingQuality,
                                          isRgba,
                                          mScaleMode
      );
      if (scalingResult.data == nullptr) {
        std::string exception = "Scaling image has failed";
        throw std::runtime_error(exception);
      }
      aligned_uint8_vector dataStore(scalingResult.length);
      *imageWidthPtr = static_cast<int>(scalingResult.width);
      *imageHeightPtr = static_cast<int>(scalingResult.height);
      *stride = static_cast<int>(scalingResult.stride);
      memcpy(
          dataStore.data(),
          scalingResult.data,
          scalingResult.length
      );
      weave_scaling_result_free(scalingResult);
      return dataStore;
    } else {
      auto scalingResult = weave_scale_u16(reinterpret_cast<const uint16_t *>(sourceData),
                                           *stride,
                                           imageWidth,
                                           imageHeight,
                                           scaledWidth,
                                           scaledHeight,
                                           bitDepth,
                                           scalingQuality,
                                           isRgba,
                                           mScaleMode
      );
      if (scalingResult.data == nullptr) {
        std::string exception = "Scaling image has failed";
        throw std::runtime_error(exception);
      }
      aligned_uint8_vector dataStore(scalingResult.length * sizeof(uint16_t));
      *imageWidthPtr = static_cast<int>(scalingResult.width);
      *imageHeightPtr = static_cast<int>(scalingResult.height);
      *stride = static_cast<int>(scalingResult.stride * 2);
      memcpy(
          dataStore.data(),
          reinterpret_cast<const uint8_t * >(scalingResult.data),
          scalingResult.length * sizeof(uint16_t)
      );
      weave_scaling_result16_free(scalingResult);
      return dataStore;
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