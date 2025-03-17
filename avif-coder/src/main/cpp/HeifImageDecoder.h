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

#ifndef AVIF_CODER_SRC_MAIN_CPP_HEIFIMAGEDECODER_H_
#define AVIF_CODER_SRC_MAIN_CPP_HEIFIMAGEDECODER_H_

#include <exception>
#include <memory>
#include "libheif/heif.h"
#include "ImageFrame.h"
#include "Support.h"
#include "SizeScaler.h"
#include "ToneMapper.h"

struct HeifUniquePtrDeleter {
  void operator()(heif_context *v) const { heif_context_free(v); }
  void operator()(heif_decoding_options *v) const { heif_decoding_options_free(v); }
  void operator()(heif_image *v) const { heif_image_release(v); }
};

class HeifImageDecoder {
 public:
  HeifImageDecoder() {
    ctx = std::unique_ptr<heif_context, HeifUniquePtrDeleter>(heif_context_alloc());
    if (!ctx) {
      throw std::runtime_error("Can't create HEIF/AVIF decoder due to unknown reason");
    }
  }

  AvifImageFrame getFrame(std::vector<uint8_t> &srcBuffer,
                          uint32_t scaledWidth,
                          uint32_t scaledHeight,
                          PreferredColorConfig javaColorSpace,
                          ScaleMode javaScaleMode,
                          int scalingQuality);

  static std::string getImageType(std::vector<uint8_t> &srcBuffer);

 private:

  std::unique_ptr<heif_context, HeifUniquePtrDeleter> ctx;
};

#endif //AVIF_CODER_SRC_MAIN_CPP_HEIFIMAGEDECODER_H_
