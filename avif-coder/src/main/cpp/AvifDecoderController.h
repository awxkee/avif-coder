//
// Created by Radzivon Bartoshyk on 12/10/2024.
//

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
    this->isBufferAttached = false;
  }

  AvifImageFrame getFrame(uint32_t frame,
                          uint32_t scaledWidth,
                          uint32_t scaledHeight,
                          PreferredColorConfig javaColorSpace,
                          ScaleMode javaScaleMode,
                          CurveToneMapper javaToneMapper,
                          bool highQualityResizer);
  void attachBuffer(uint8_t *data, uint32_t bufferSize);
  uint32_t getFramesCount();
  uint32_t getLoopsCount();
  uint32_t getTotalDuration();
  uint32_t getFrameDuration(uint32_t frame);
  AvifImageSize getImageSize();

 private:
  bool isBufferAttached;
  aligned_uint8_vector buffer;
  avif::DecoderPtr decoder;
  std::mutex mutex;
};

#endif //AVIF_CODER_SRC_MAIN_CPP_AVIFDECODERCONTROLLER_H_
