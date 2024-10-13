//
// Created by Radzivon Bartoshyk on 13/10/2024.
//

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
                          CurveToneMapper javaToneMapper,
                          bool highQualityResizer);

  static std::string getImageType(std::vector<uint8_t> &srcBuffer);

 private:

  std::unique_ptr<heif_context, HeifUniquePtrDeleter> ctx;
};

#endif //AVIF_CODER_SRC_MAIN_CPP_HEIFIMAGEDECODER_H_
