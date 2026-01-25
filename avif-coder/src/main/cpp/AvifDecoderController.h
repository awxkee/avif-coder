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

/**
 * Controller class for decoding AVIF images, including animated sequences.
 * 
 * This class provides thread-safe access to AVIF decoder functionality.
 * It manages the lifecycle of the underlying libavif decoder and provides
 * methods to decode frames, get image metadata, and handle animated sequences.
 * 
 * Thread safety: Methods are protected by internal mutex for concurrent access.
 */
class AvifDecoderController {
 public:
  /**
   * Creates a new AVIF decoder controller.
   * Buffer must be attached using attachBuffer() before use.
   * 
   * @throws std::runtime_error if decoder creation fails
   */
  AvifDecoderController() {
    this->decoder = avif::DecoderPtr(avifDecoderCreate());
    if (!decoder) {
      throw std::runtime_error("Can't create decoder");
    }
    this->isBufferAttached = false;
  }

  /**
   * Creates a new AVIF decoder controller and attaches the buffer.
   * 
   * @param data Pointer to the image data
   * @param bufferSize Size of the image data in bytes
   * @throws std::runtime_error if decoder creation or buffer attachment fails
   */
  AvifDecoderController(uint8_t *data, uint32_t bufferSize) {
    this->decoder = avif::DecoderPtr(avifDecoderCreate());
    this->isBufferAttached = false;
    this->attachBuffer(data, bufferSize);
  }

  /**
   * Gets a decoded frame from the AVIF image with optional scaling.
   * 
   * @param frame Frame index (0-based)
   * @param scaledWidth Target width (0 means no scaling)
   * @param scaledHeight Target height (0 means no scaling)
   * @param javaColorSpace Preferred color configuration
   * @param javaScaleMode Scaling mode (FIT or FILL)
   * @param scalingQuality Quality of scaling
   * @return Decoded image frame
   * @throws std::runtime_error if buffer not attached, frame index out of bounds, or decoding fails
   */
  AvifImageFrame getFrame(uint32_t frame,
                          uint32_t scaledWidth,
                          uint32_t scaledHeight,
                          PreferredColorConfig javaColorSpace,
                          ScaleMode javaScaleMode,
                          int scalingQuality);
  
  /**
   * Attaches image data buffer to the decoder.
   * Can only be called once per controller instance.
   * 
   * @param data Pointer to the image data (must not be null)
   * @param bufferSize Size of the image data in bytes (must be > 0)
   * @throws std::runtime_error if buffer already attached, data is null, size is 0, or parsing fails
   */
  void attachBuffer(uint8_t *data, uint32_t bufferSize);
  
  /**
   * Gets the total number of frames in the image.
   * 
   * @return Number of frames
   * @throws std::runtime_error if buffer not attached
   */
  uint32_t getFramesCount();
  
  /**
   * Gets the loop count for animated images.
   * 
   * @return Loop count (0 means infinite loop)
   * @throws std::runtime_error if buffer not attached
   */
  uint32_t getLoopsCount();
  
  /**
   * Gets the total duration of the animation in milliseconds.
   * 
   * @return Total duration in milliseconds
   * @throws std::runtime_error if buffer not attached
   */
  uint32_t getTotalDuration();
  
  /**
   * Gets the duration of a specific frame in milliseconds.
   * 
   * @param frame Frame index (0-based)
   * @return Frame duration in milliseconds
   * @throws std::runtime_error if buffer not attached or frame index out of bounds
   */
  uint32_t getFrameDuration(uint32_t frame);
  
  /**
   * Gets the dimensions of the image.
   * 
   * @return Image size (width and height)
   * @throws std::runtime_error if buffer not attached
   */
  AvifImageSize getImageSize();

  /**
   * Static method to get image dimensions without creating a full controller.
   * 
   * @param data Pointer to the image data (must not be null)
   * @param bufferSize Size of the image data in bytes (must be > 0)
   * @return Image size (width and height)
   * @throws std::runtime_error if data is null, size is 0, or image cannot be parsed
   */
  static AvifImageSize getImageSize(uint8_t *data, uint32_t bufferSize);

 private:
  bool isBufferAttached;
  aligned_uint8_vector buffer;
  avif::DecoderPtr decoder;
  std::mutex mutex;
};

#endif //AVIF_CODER_SRC_MAIN_CPP_AVIFDECODERCONTROLLER_H_
