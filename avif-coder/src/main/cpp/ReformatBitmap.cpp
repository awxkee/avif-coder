/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 16/9/2023
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

#include "ReformatBitmap.h"
#include "imagebits/Rgba8ToF16.h"
#include "imagebits/Rgb565.h"
#include "imagebits/Rgb1010102.h"
#include "imagebits/CopyUnalignedRGBA.h"
#include "imagebits/Rgba16.h"
#include <string>
#include <atomic>
#include "Support.h"
#include "JniException.h"
#include <android/bitmap.h>
#include <HardwareBuffersCompat.h>
#include <mutex>
#include <android/log.h>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>
#include <utility>
#include "imagebits/RGBAlpha.h"
#include "avifweaver.h"

using namespace std;

namespace {
constexpr const char *kHardwareBufferLogTag = "AvifCoder";
std::atomic_bool gHardwareBufferDebugLoggingEnabled{false};

struct HardwareBufferReleaser {
  void operator()(AHardwareBuffer *buffer) const noexcept {
    if (buffer != nullptr && AHardwareBuffer_release_compat != nullptr) {
      AHardwareBuffer_release_compat(buffer);
    }
  }
};

using HardwareBufferPtr = std::unique_ptr<AHardwareBuffer, HardwareBufferReleaser>;

std::string hardwareBufferStatusString(int status) {
  const int64_t signedStatus = status;
  const int64_t errnoValue = signedStatus < 0 ? -signedStatus : signedStatus;
  std::ostringstream stream;
  stream << status;
  if (errnoValue > 0 && errnoValue <= std::numeric_limits<int>::max()) {
    stream << " (errno " << errnoValue << ": "
           << std::strerror(static_cast<int>(errnoValue)) << ")";
  }
  return stream.str();
}

std::string hardwareBufferDescString(const AHardwareBuffer_Desc &desc) {
  std::ostringstream stream;
  stream << desc.width << 'x' << desc.height
         << ", layers=" << desc.layers
         << ", format=" << desc.format
         << ", usage=0x" << std::hex << desc.usage << std::dec
         << ", stride=" << desc.stride << "px";
  return stream.str();
}

bool checkedImageByteCount(uint32_t stride, uint32_t height, size_t *byteCount) {
  if (height != 0 && stride > std::numeric_limits<size_t>::max() / height) {
    return false;
  }
  *byteCount = static_cast<size_t>(stride) * height;
  return true;
}
}

namespace coder {
void SetHardwareBufferDebugLoggingEnabled(bool enabled) noexcept {
  gHardwareBufferDebugLoggingEnabled.store(enabled, std::memory_order_relaxed);
}

bool IsHardwareBufferDebugLoggingEnabled() noexcept {
  return gHardwareBufferDebugLoggingEnabled.load(std::memory_order_relaxed);
}

void
ReformatColorConfig(JNIEnv *env, aligned_uint8_vector &imageData, string &imageConfig,
                    PreferredColorConfig preferredColorConfig, uint32_t depth,
                    uint32_t imageWidth, uint32_t imageHeight, uint32_t *stride, bool *useFloats,
                    jobject *hwBuffer, bool alphaPremultiplied, bool doesImageHasAlpha) {
  *hwBuffer = nullptr;

  if (!alphaPremultiplied && doesImageHasAlpha) {
    if (!(*useFloats)) {
      coder::AssociateAlphaRgba8(imageData.data(), *stride,
                                 imageData.data(), *stride,
                                 imageWidth,
                                 imageHeight);
    } else {
      coder::AssociateAlphaRgba16(reinterpret_cast<uint16_t *>(imageData.data()), *stride,
                                  reinterpret_cast<uint16_t *>(imageData.data()), *stride,
                                  imageWidth,
                                  imageHeight, depth);
    }
  }

  switch (preferredColorConfig) {
    case Rgba_8888:
      if (*useFloats) {
        uint32_t
            lineWidth = imageWidth * 4 * (uint32_t)
            sizeof(uint8_t);
        uint32_t alignment = 64;
        uint32_t padding = (alignment - (lineWidth % alignment)) % alignment;
        uint32_t dstStride = lineWidth + padding;
        aligned_uint8_vector rgba8888Data(dstStride * imageHeight);
        coder::Rgba16ToRgba8(reinterpret_cast<const uint16_t *>(imageData.data()),
                             *stride, rgba8888Data.data(), dstStride, imageWidth,
                             imageHeight, depth);
        *stride = dstStride;
        *useFloats = false;
        imageConfig = "ARGB_8888";
        imageData = std::move(rgba8888Data);
      }
      break;
    case Rgba_F16:
      if (*useFloats) {
        weave_cvt_rgba16_to_rgba_f16(reinterpret_cast<const uint16_t *>(imageData.data()),
                                     *stride,
                                     depth,
                                     reinterpret_cast<uint16_t *>(imageData.data()),
                                     *stride,
                                     imageWidth, imageHeight);
      } else {
        uint32_t
            lineWidth = imageWidth * 4 * (uint32_t)
            sizeof(uint16_t);
        uint32_t dstStride = lineWidth;
        aligned_uint8_vector rgbaF16Data(dstStride * imageHeight);
        weave_cvt_rgba8_to_rgba_f16(
            imageData.data(), *stride,
            reinterpret_cast<uint16_t *>(rgbaF16Data.data()), dstStride,
            imageWidth, imageHeight);
        *stride = dstStride;
        *useFloats = true;
        imageConfig = "RGBA_F16";
        imageData = std::move(rgbaF16Data);
      }
      break;
    case Rgb_565:
      if (*useFloats) {
        uint32_t
            lineWidth = imageWidth * (uint32_t)
            sizeof(uint16_t);
        uint32_t alignment = 64;
        uint32_t padding = (alignment - (lineWidth % alignment)) % alignment;
        uint32_t dstStride = lineWidth + padding;
        aligned_uint8_vector rgb565Data(dstStride * imageHeight);
        coder::Rgba16To565(reinterpret_cast<const uint16_t *>(imageData.data()),
                           *stride,
                           reinterpret_cast<uint16_t *>(rgb565Data.data()), dstStride,
                           imageWidth, imageHeight, depth);
        *stride = dstStride;
        *useFloats = false;
        imageConfig = "RGB_565";
        imageData = std::move(rgb565Data);
        break;
      } else {
        uint32_t lineWidth = imageWidth * (uint32_t)
            sizeof(uint16_t);
        uint32_t alignment = 64;
        uint32_t padding = (alignment - (lineWidth % alignment)) % alignment;
        uint32_t dstStride = lineWidth + padding;
        aligned_uint8_vector rgb565Data(dstStride * imageHeight);
        coder::Rgba8To565(imageData.data(), *stride,
                          reinterpret_cast<uint16_t *>(rgb565Data.data()), dstStride,
                          imageWidth, imageHeight,
                          !alphaPremultiplied);
        *stride = dstStride;
        *useFloats = false;
        imageConfig = "RGB_565";
        imageData = std::move(rgb565Data);
      }
      break;
    case Rgba_1010102:
      if (*useFloats) {
        uint32_t
            lineWidth = imageWidth * (uint32_t)
            sizeof(uint32_t);
        uint32_t dstStride = lineWidth;
        aligned_uint8_vector rgba1010102Data(dstStride * imageHeight);
        weave_cvt_rgba16_to_ar30(reinterpret_cast<const uint16_t *>(imageData.data()),
                                 *stride,
                                 depth,
                                 reinterpret_cast<uint8_t *>(rgba1010102Data.data()),
                                 dstStride,
                                 imageWidth, imageHeight);
        *stride = dstStride;
        *useFloats = false;
        imageConfig = "RGBA_1010102";
        imageData = std::move(rgba1010102Data);
        break;
      } else {
        uint32_t
            dstStride = imageWidth * (uint32_t)
            sizeof(uint32_t) * 4;
        aligned_uint8_vector rgba1010102Data(dstStride * imageHeight);
        weave_cvt_rgba8_to_ar30(reinterpret_cast<const uint8_t *>(imageData.data()),
                                *stride,
                                reinterpret_cast<uint8_t *>(rgba1010102Data.data()),
                                dstStride,
                                imageWidth, imageHeight);
        *stride = dstStride;
        *useFloats = false;
        imageConfig = "RGBA_1010102";
        imageData = std::move(rgba1010102Data);
        break;
      }
      break;
    case Hardware: {
      const uint32_t bytesPerPixel = (*useFloats) ? 4u * sizeof(uint16_t)
                                                  : 4u * sizeof(uint8_t);
      const uint64_t minimumRowBytes64 = static_cast<uint64_t>(imageWidth) * bytesPerPixel;
      size_t sourceByteCount = 0;
      if (imageWidth == 0 || imageHeight == 0
          || minimumRowBytes64 > std::numeric_limits<uint32_t>::max()
          || *stride < minimumRowBytes64
          || !checkedImageByteCount(*stride, imageHeight, &sourceByteCount)
          || imageData.size() < sourceByteCount) {
        std::ostringstream stream;
        stream << "Invalid source layout for hardware upload: image="
               << imageWidth << 'x' << imageHeight
               << ", stride=" << *stride
               << ", minimum_row_bytes=" << minimumRowBytes64
               << ", source_bytes=" << imageData.size();
        throw std::runtime_error(stream.str());
      }

      auto useSoftwareFallback = [&](const std::string &reason) {
        __android_log_print(ANDROID_LOG_WARN, kHardwareBufferLogTag,
                            "Hardware bitmap upload failed; using software bitmap. "
                            "api=%d, image=%ux%u, stride=%u, source_bytes=%zu, reason=%s",
                            androidOSVersion(), imageWidth, imageHeight, *stride,
                            imageData.size(), reason.c_str());
        *hwBuffer = nullptr;
        if (*useFloats) {
          weave_cvt_rgba16_to_rgba_f16(
              reinterpret_cast<const uint16_t *>(imageData.data()),
              *stride,
              depth,
              reinterpret_cast<uint16_t *>(imageData.data()),
              *stride,
              imageWidth,
              imageHeight);
          imageConfig = "RGBA_F16";
        } else {
          imageConfig = "ARGB_8888";
        }
      };

      if (!loadAHardwareBuffersAPI()) {
        useSoftwareFallback("AHardwareBuffer API is unavailable");
        return;
      }

      AHardwareBuffer_Desc requestedDesc = {0};
      requestedDesc.width = imageWidth;
      requestedDesc.height = imageHeight;
      requestedDesc.layers = 1;
      requestedDesc.format = (*useFloats) ? AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT
                                          : AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
      // The bitmap is uploaded once by the CPU and then sampled by the GPU.
      // The CPU usage used for locking must also be declared at allocation.
      requestedDesc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE
          | AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY;

      if (IsHardwareBufferDebugLoggingEnabled()) {
        __android_log_print(ANDROID_LOG_DEBUG, kHardwareBufferLogTag,
                            "AHardwareBuffer allocation request: api=%d, desc={%s}, "
                            "source_stride=%u, source_bytes=%zu",
                            androidOSVersion(),
                            hardwareBufferDescString(requestedDesc).c_str(),
                            *stride, imageData.size());
      }

      if (AHardwareBuffer_isSupported_compat(&requestedDesc) == 0) {
        useSoftwareFallback("AHardwareBuffer descriptor is not supported: "
                                + hardwareBufferDescString(requestedDesc));
        return;
      }

      AHardwareBuffer *rawHardwareBuffer = nullptr;
      int status = AHardwareBuffer_allocate_compat(&requestedDesc, &rawHardwareBuffer);
      if (status != 0 || rawHardwareBuffer == nullptr) {
        std::ostringstream stream;
        stream << "AHardwareBuffer_allocate failed: status="
               << hardwareBufferStatusString(status)
               << ", buffer=" << rawHardwareBuffer
               << ", requested={" << hardwareBufferDescString(requestedDesc) << '}';
        useSoftwareFallback(stream.str());
        return;
      }
      HardwareBufferPtr hardwareBuffer(rawHardwareBuffer);

      AHardwareBuffer_Desc actualDesc = {0};
      AHardwareBuffer_describe_compat(hardwareBuffer.get(), &actualDesc);
      if (actualDesc.width != imageWidth
          || actualDesc.height != imageHeight
          || actualDesc.layers != 1
          || actualDesc.format != requestedDesc.format
          || actualDesc.stride < imageWidth) {
        std::ostringstream stream;
        stream << "Allocated descriptor mismatch: requested={"
               << hardwareBufferDescString(requestedDesc)
               << "}, actual={" << hardwareBufferDescString(actualDesc) << '}';
        useSoftwareFallback(stream.str());
        return;
      }

      const uint64_t destinationStride64 = static_cast<uint64_t>(actualDesc.stride)
          * bytesPerPixel;
      if (actualDesc.height != 0
          && destinationStride64 > std::numeric_limits<uint64_t>::max() / actualDesc.height) {
        useSoftwareFallback("Allocated hardware mapped byte count overflows uint64_t: actual={"
                                + hardwareBufferDescString(actualDesc) + '}');
        return;
      }
      const uint64_t mappedBytes64 = destinationStride64 * actualDesc.height;
      if (destinationStride64 > std::numeric_limits<uint32_t>::max()
          || mappedBytes64 > std::numeric_limits<size_t>::max()) {
        std::ostringstream stream;
        stream << "Allocated hardware layout overflows addressable sizes: actual={"
               << hardwareBufferDescString(actualDesc)
               << "}, destination_stride=" << destinationStride64
               << ", mapped_bytes=" << mappedBytes64;
        useSoftwareFallback(stream.str());
        return;
      }

      uint8_t *buffer = nullptr;
      const uint64_t lockUsage = AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY;
      // nullptr means the complete buffer and avoids vendor-specific validation
      // of a redundant full-size dirty rectangle.
      status = AHardwareBuffer_lock_compat(hardwareBuffer.get(), lockUsage, -1,
                                           nullptr, reinterpret_cast<void **>(&buffer));
      if (status != 0 || buffer == nullptr) {
        std::ostringstream stream;
        stream << "AHardwareBuffer_lock failed: status="
               << hardwareBufferStatusString(status)
               << ", address=" << static_cast<void *>(buffer)
               << ", lock_usage=0x" << std::hex << lockUsage << std::dec
               << ", requested={" << hardwareBufferDescString(requestedDesc)
               << "}, actual={" << hardwareBufferDescString(actualDesc)
               << "}, destination_stride=" << destinationStride64
               << ", mapped_bytes=" << mappedBytes64;
        if (status == 0) {
          const int unlockStatus = AHardwareBuffer_unlock_compat(hardwareBuffer.get(), nullptr);
          if (unlockStatus != 0) {
            stream << ", cleanup_unlock_status="
                   << hardwareBufferStatusString(unlockStatus);
          }
        }
        useSoftwareFallback(stream.str());
        return;
      }

      if (*useFloats) {
        weave_cvt_rgba16_to_rgba_f16(reinterpret_cast<const uint16_t *>(imageData.data()),
                                     *stride,
                                     depth,
                                     reinterpret_cast<uint16_t *>(buffer),
                                     static_cast<uint32_t>(destinationStride64),
                                     imageWidth,
                                     imageHeight);
      } else {
        CopyUnaligned(reinterpret_cast<const uint8_t *>(imageData.data()),
                      *stride,
                      reinterpret_cast<uint8_t *>(buffer),
                      static_cast<uint32_t>(destinationStride64),
                      imageWidth * 4,
                      imageHeight);
      }

      status = AHardwareBuffer_unlock_compat(hardwareBuffer.get(), nullptr);
      if (status != 0) {
        std::ostringstream stream;
        stream << "AHardwareBuffer_unlock failed after upload: status="
               << hardwareBufferStatusString(status)
               << ", requested={" << hardwareBufferDescString(requestedDesc)
               << "}, actual={" << hardwareBufferDescString(actualDesc) << '}';
        useSoftwareFallback(stream.str());
        return;
      }

      jobject buf = AHardwareBuffer_toHardwareBuffer_compat(env, hardwareBuffer.get());
      if (buf == nullptr) {
        if (env->ExceptionCheck()) {
          __android_log_print(ANDROID_LOG_ERROR, kHardwareBufferLogTag,
                              "AHardwareBuffer_toHardwareBuffer returned null with a pending "
                              "JNI exception; image=%ux%u, actual={%s}",
                              imageWidth, imageHeight,
                              hardwareBufferDescString(actualDesc).c_str());
          return;
        }
        useSoftwareFallback("AHardwareBuffer_toHardwareBuffer returned null");
        return;
      }

      if (IsHardwareBufferDebugLoggingEnabled()) {
        __android_log_print(ANDROID_LOG_DEBUG, kHardwareBufferLogTag,
                            "AHardwareBuffer upload completed: requested={%s}, actual={%s}, "
                            "destination_stride=%llu, mapped_bytes=%llu",
                            hardwareBufferDescString(requestedDesc).c_str(),
                            hardwareBufferDescString(actualDesc).c_str(),
                            static_cast<unsigned long long>(destinationStride64),
                            static_cast<unsigned long long>(mappedBytes64));
      }

      *hwBuffer = buf;
      imageConfig = "HARDWARE";
    }
      break;
    default: {
      if (*useFloats) {
        weave_cvt_rgba16_to_rgba_f16(reinterpret_cast<const uint16_t *>(imageData.data()),
                                     *stride,
                                     depth,
                                     reinterpret_cast<uint16_t *>(imageData.data()),
                                     *stride,
                                     imageWidth, imageHeight);
      }
    }
      break;
  }
}
}