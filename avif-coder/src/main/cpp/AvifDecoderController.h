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

#ifndef AVIF_CODER_SRC_MAIN_CPP_AVIFDECODERCONTROLLER_H_
#define AVIF_CODER_SRC_MAIN_CPP_AVIFDECODERCONTROLLER_H_

#include "avif/avif_cxx.h"
#include <vector>
#include "definitions.h"
#include "SizeScaler.h"
#include "Support.h"
#include <thread>
#include "ImageFrame.h"

class AvifDecoderController {
 public:
  AvifDecoderController() {
    this->decoder = avif::DecoderPtr(avifDecoderCreate());
    if (!decoder) {
      throw std::runtime_error("Can't create decoder");
    }
    this->isBufferAttached = false;
  }

  AvifDecoderController(uint8_t *data, uint32_t bufferSize) {
    this->decoder = avif::DecoderPtr(avifDecoderCreate());
    this->isBufferAttached = false;
    this->attachBuffer(data, bufferSize);
  }

  AvifImageFrame getFrame(uint32_t frame,
                          uint32_t scaledWidth,
                          uint32_t scaledHeight,
                          PreferredColorConfig javaColorSpace,
                          ScaleMode javaScaleMode,
                          int scalingQuality);
  void attachBuffer(uint8_t *data, uint32_t bufferSize);
  uint32_t getFramesCount();
  uint32_t getLoopsCount();
  uint32_t getTotalDuration();
  uint32_t getFrameDuration(uint32_t frame);
  AvifImageSize getImageSize();

  static AvifImageSize getImageSize(uint8_t *data, uint32_t bufferSize);

 private:
  bool isBufferAttached;
  aligned_uint8_vector buffer;
  avif::DecoderPtr decoder;
  std::mutex mutex;
};

#endif //AVIF_CODER_SRC_MAIN_CPP_AVIFDECODERCONTROLLER_H_
