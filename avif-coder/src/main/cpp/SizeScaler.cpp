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
#include <string>
#include <jni.h>
#include "JniException.h"
#include "definitions.h"
#include "avifweaver.h"

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