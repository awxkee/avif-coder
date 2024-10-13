/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 15/9/2023
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

#include <jni.h>
#include <string>
#include "libheif/heif.h"
#include "android/bitmap.h"
#include <vector>
#include "JniException.h"
#include "SizeScaler.h"
#include <android/log.h>
#include <android/data_space.h>
#include <sys/system_properties.h>
#include "icc/lcms2.h"
#include "colorspace/colorspace.h"
#include <cmath>
#include <limits>
#include "imagebits/RgbaF16bitToNBitU16.h"
#include "imagebits/RgbaF16bitNBitU8.h"
#include "imagebits/Rgb1010102.h"
#include "imagebits/RGBAlpha.h"
#include "imagebits/Rgb565.h"
#include "DataSpaceToNCLX.hpp"
#include "imagebits/CopyUnalignedRGBA.h"
#include "imagebits/ScanAlpha.h"
#include "YuvConversion.h"
#include "avif/avif.h"
#include "avif/avif_cxx.h"
#include <libyuv.h>
#include "AvifDecoderController.h"

using namespace std;

struct AvifMemEncoder {
  std::vector<char> buffer;
};

enum AvifQualityMode {
  AVIF_LOSSY_MODE = 1,
  AVIF_LOSELESS_MODE = 2
};

enum AvifEncodingSurface {
  RGB, RGBA, AUTO
};

enum AvifChromaSubsampling {
  AVIF_CHROMA_AUTO,
  AVIF_CHROMA_YUV_420,
  AVIF_CHROMA_YUV_422,
  AVIF_CHROMA_YUV_444,
  AVIF_CHROMA_YUV_400
};

struct heif_error writeHeifData(struct heif_context *ctx,
                                const void *data,
                                size_t size,
                                void *userdata) {
  auto *p = (struct AvifMemEncoder *) userdata;
  p->buffer.insert(p->buffer.end(),
                   reinterpret_cast<const uint8_t *>(data),
                   reinterpret_cast<const uint8_t *>(data) + size);

  struct heif_error
      error_ok;
  error_ok.code = heif_error_Ok;
  error_ok.subcode = heif_suberror_Unspecified;
  error_ok.message = "ok";
  return (error_ok);
}

jbyteArray encodeBitmapHevc(JNIEnv *env,
                            jobject thiz,
                            jobject bitmap,
                            const int quality,
                            const int dataSpace,
                            const bool loseless,
                            std::string &x265Preset,
                            const int crf) {
  std::shared_ptr<heif_context> ctx(heif_context_alloc(),
                                    [](heif_context *c) { heif_context_free(c); });
  if (!ctx) {
    std::string exception = "Can't create HEIF/AVIF encoder due to unknown reason";
    throwException(env, exception);
    return static_cast<jbyteArray>(nullptr);
  }

  heif_encoder *mEncoder;
  auto result = heif_context_get_encoder_for_format(ctx.get(), heif_compression_HEVC, &mEncoder);
  if (result.code != heif_error_Ok) {
    std::string choke(result.message);
    std::string str = "Can't create encoder with exception: " + choke;
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  std::shared_ptr<heif_encoder> encoder(mEncoder,
                                        [](heif_encoder *he) { heif_encoder_release(he); });

  if (loseless) {
    result = heif_encoder_set_lossless(encoder.get(), loseless);
    if (result.code != heif_error_Ok) {
      std::string choke(result.message);
      std::string str = "Can't create encoder with exception: " + choke;
      throwException(env, str);
      return static_cast<jbyteArray>(nullptr);
    }

  } else {
    result = heif_encoder_set_lossy_quality(encoder.get(), quality);
    if (result.code != heif_error_Ok) {
      std::string choke(result.message);
      std::string str = "Can't create encoder with exception: " + choke;
      throwException(env, str);
      return static_cast<jbyteArray>(nullptr);
    }
  }

  AndroidBitmapInfo info;
  if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
    throwPixelsException(env);
    return static_cast<jbyteArray>(nullptr);
  }

  if (info.flags & ANDROID_BITMAP_FLAGS_IS_HARDWARE) {
    throwHardwareBitmapException(env);
    return static_cast<jbyteArray>(nullptr);
  }

  if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 &&
      info.format != ANDROID_BITMAP_FORMAT_RGB_565 &&
      info.format != ANDROID_BITMAP_FORMAT_RGBA_F16 &&
      info.format != ANDROID_BITMAP_FORMAT_RGBA_1010102) {
    throwInvalidPixelsFormat(env);
    return static_cast<jbyteArray>(nullptr);
  }

  void *addr;
  if (AndroidBitmap_lockPixels(env, bitmap, &addr) != 0) {
    throwPixelsException(env);
    return static_cast<jbyteArray>(nullptr);
  }

  std::vector<uint8_t> sourceData(info.height * info.stride);
  std::copy(reinterpret_cast<uint8_t *>(addr),
            reinterpret_cast<uint8_t *>(addr) + info.height * info.stride, sourceData.data());

  AndroidBitmap_unlockPixels(env, bitmap);

  result = heif_encoder_set_parameter(encoder.get(), "x265:preset", x265Preset.c_str());
  if (result.code != heif_error_Ok) {
    std::string choke(result.message);
    std::string str = "Can't create encoded image with exception: " + choke;
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  auto crfString = std::to_string(crf);
  result = heif_encoder_set_parameter(encoder.get(), "x265:crf", crfString.c_str());
  if (result.code != heif_error_Ok) {
    std::string choke(result.message);
    std::string str = "Can't create encoded image with exception: " + choke;
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  heif_image *imagePtr;

  result = heif_image_create((int) info.width, (int) info.height,
                             heif_colorspace_YCbCr, heif_chroma_420, &imagePtr);
  if (result.code != heif_error_Ok) {
    std::string choke(result.message);
    std::string str = "Can't create encoded image with exception: " + choke;
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  std::shared_ptr<heif_image> image(imagePtr, [](auto ptr) {
    heif_image_release(ptr);
  });

  std::shared_ptr<heif_color_profile_nclx> profile(heif_nclx_color_profile_alloc(), [](auto x) {
    heif_nclx_color_profile_free(x);
  });

  int bitDepth = 8;

  result = heif_image_add_plane(image.get(),
                                heif_channel_Y,
                                (int) info.width,
                                (int) info.height,
                                bitDepth);
  if (result.code != heif_error_Ok) {
    std::string choke(result.message);
    std::string str = "Can't create add plane to encoded image with exception: " + choke;
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  int uvPlaneWidth = ((int) info.width + 1) / 2;
  int uvPlaneHeight = ((int) info.height + 1) / 2;

  result = heif_image_add_plane(image.get(),
                                heif_channel_Cb,
                                uvPlaneWidth,
                                uvPlaneHeight,
                                bitDepth);
  if (result.code != heif_error_Ok) {
    std::string choke(result.message);
    std::string str = "Can't create add plane to encoded image with exception: " + choke;
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  result = heif_image_add_plane(image.get(),
                                heif_channel_Cr,
                                uvPlaneWidth,
                                uvPlaneHeight,
                                bitDepth);
  if (result.code != heif_error_Ok) {
    std::string choke(result.message);
    std::string str = "Can't create add plane to encoded image with exception: " + choke;
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  uint32_t stride = info.width * 4;
  aligned_uint8_vector imageStore(stride * info.height);
  if (info.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
    coder::UnassociateRgba8(sourceData.data(), (int) info.stride,
                            imageStore.data(), stride, (int) info.width, (int) info.height);
    heif_image_set_premultiplied_alpha(image.get(), false);
  } else if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
    coder::Rgb565ToUnsigned8(reinterpret_cast<uint16_t *>(sourceData.data()), (int) info.stride,
                             imageStore.data(), stride, (int) info.width, (int) info.height, 255);
  } else if (info.format == ANDROID_BITMAP_FORMAT_RGBA_1010102) {
    coder::RGBA1010102ToUnsigned(reinterpret_cast<uint8_t *>(sourceData.data()),
                                 (uint32_t) info.stride,
                                 reinterpret_cast<uint8_t *>(imageStore.data()), stride,
                                 (uint32_t) info.width, (uint32_t) info.height, 8);
    heif_image_set_premultiplied_alpha(image.get(), true);
  } else if (info.format == ANDROID_BITMAP_FORMAT_RGBA_F16) {
    coder::RGBAF16BitToNBitU8(reinterpret_cast<const uint16_t *>(sourceData.data()),
                              (int) info.stride,
                              reinterpret_cast<uint8_t *>(imageStore.data()), stride,
                              (int) info.width,
                              (int) info.height, 8, true);
    heif_image_set_premultiplied_alpha(image.get(), true);
  }

  int yStride;
  uint8_t *yPlane = heif_image_get_plane(image.get(), heif_channel_Y, &yStride);
  if (yPlane == nullptr) {
    std::string str = "Can't add Y plane to an image";
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  int uStride;
  uint8_t *uPlane = heif_image_get_plane(image.get(), heif_channel_Cb, &uStride);
  if (uPlane == nullptr) {
    std::string str = "Can't add U plane to an image";
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  int vStride;
  uint8_t *vPlane = heif_image_get_plane(image.get(), heif_channel_Cr, &vStride);
  if (vPlane == nullptr) {
    std::string str = "Can't add V plane to an image";
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  std::vector<uint8_t> iccProfile(0);

  YuvMatrix matrix = YuvMatrix::Bt709;

  bool nclxResult = coder::colorProfileFromDataSpace(dataSpace,
                                                     profile.get(),
                                                     iccProfile, matrix);

  if (!nclxResult) {
    profile->full_range_flag = 1;
  }

  RgbaToYuv420(imageStore.data(),
               stride,
               yPlane,
               yStride,
               uPlane,
               uStride,
               vPlane,
               vStride,
               info.width,
               info.height,
               profile->full_range_flag ? YuvRange::Full : YuvRange::Tv,
               matrix);

  if (nclxResult) {
    if (iccProfile.empty()) {
      result = heif_image_set_nclx_color_profile(image.get(), profile.get());
      if (result.code != heif_error_Ok) {
        std::string choke(result.message);
        std::string str = "Can't set required color profiel: " + choke;
        throwException(env, str);
        return static_cast<jbyteArray>(nullptr);
      }
    } else {
      result =
          heif_image_set_raw_color_profile(image.get(),
                                           "prof",
                                           iccProfile.data(),
                                           iccProfile.size());
      if (result.code != heif_error_Ok) {
        std::string choke(result.message);
        std::string str = "Can't set required color profile: " + choke;
        throwException(env, str);
        return static_cast<jbyteArray>(nullptr);
      }
    }
  }

  heif_image_handle *handle;
  std::shared_ptr<heif_encoding_options> options(heif_encoding_options_alloc(),
                                                 [](heif_encoding_options *o) {
                                                   heif_encoding_options_free(o);
                                                 });
  options->version = 5;
  options->image_orientation = heif_orientation_normal;

  result = heif_context_encode_image(ctx.get(), image.get(), encoder.get(), options.get(), &handle);
  options.reset();
  if (handle && result.code == heif_error_Ok) {
    heif_context_set_primary_image(ctx.get(), handle);
    heif_image_handle_release(handle);
  }
  image.reset();
  if (result.code != heif_error_Ok) {
    std::string choke(result.message);
    std::string str = "Encoding an image failed with exception: " + choke;
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  encoder.reset();

  std::vector<char> buf;
  heif_writer writer = {};
  writer.writer_api_version = 1;
  writer.write = writeHeifData;
  AvifMemEncoder memEncoder;
  result = heif_context_write(ctx.get(), &writer, &memEncoder);
  if (result.code != heif_error_Ok) {
    std::string choke(result.message);
    std::string str = "Writing encoded image has failed with exception: " + choke;
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  jbyteArray byteArray = env->NewByteArray((jsize) memEncoder.buffer.size());
  char *memBuf = (char *) ((void *) memEncoder.buffer.data());
  env->SetByteArrayRegion(byteArray, 0, (jint) memEncoder.buffer.size(),
                          reinterpret_cast<const jbyte *>(memBuf));
  return byteArray;
}

jbyteArray encodeBitmapAvif(JNIEnv *env,
                            jobject thiz,
                            jobject bitmap,
                            const int quality,
                            const int dataSpace,
                            const AvifQualityMode qualityMode,
                            const AvifEncodingSurface surface,
                            const int speed,
                            const AvifChromaSubsampling preferredChromaSubsampling) {
  avif::EncoderPtr encoder(avifEncoderCreate());
  if (encoder == nullptr) {
    std::string str = "Can't create encoder";
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  if (qualityMode == AVIF_LOSSY_MODE) {
    encoder->quality = quality;
    encoder->qualityAlpha = quality;
  } else if (qualityMode == AVIF_LOSELESS_MODE) {
    encoder->quality = 100;
    encoder->qualityAlpha = 100;
  }

  encoder->speed = std::clamp(speed, AVIF_SPEED_SLOWEST, AVIF_SPEED_FASTEST);

  AndroidBitmapInfo info;
  if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
    throwPixelsException(env);
    return static_cast<jbyteArray>(nullptr);
  }

  if (info.flags & ANDROID_BITMAP_FLAGS_IS_HARDWARE) {
    throwHardwareBitmapException(env);
    return static_cast<jbyteArray>(nullptr);
  }

  if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 &&
      info.format != ANDROID_BITMAP_FORMAT_RGB_565 &&
      info.format != ANDROID_BITMAP_FORMAT_RGBA_F16 &&
      info.format != ANDROID_BITMAP_FORMAT_RGBA_1010102) {
    throwInvalidPixelsFormat(env);
    return static_cast<jbyteArray>(nullptr);
  }

  void *addr;
  if (AndroidBitmap_lockPixels(env, bitmap, &addr) != 0) {
    throwPixelsException(env);
    return static_cast<jbyteArray>(nullptr);
  }

  std::vector<uint8_t> sourceData(info.height * info.stride);
  std::copy(reinterpret_cast<uint8_t *>(addr),
            reinterpret_cast<uint8_t *>(addr) + info.height * info.stride, sourceData.data());

  AndroidBitmap_unlockPixels(env, bitmap);

  avifPixelFormat pixelFormat = avifPixelFormat::AVIF_PIXEL_FORMAT_YUV420;
  if (preferredChromaSubsampling == AvifChromaSubsampling::AVIF_CHROMA_AUTO) {
    if (qualityMode == AVIF_LOSELESS_MODE || quality > 93) {
      pixelFormat = avifPixelFormat::AVIF_PIXEL_FORMAT_YUV444;
    } else if (quality > 65) {
      pixelFormat = avifPixelFormat::AVIF_PIXEL_FORMAT_YUV422;
    }
  } else if (preferredChromaSubsampling == AvifChromaSubsampling::AVIF_CHROMA_YUV_422) {
    pixelFormat = avifPixelFormat::AVIF_PIXEL_FORMAT_YUV422;
  } else if (preferredChromaSubsampling == AvifChromaSubsampling::AVIF_CHROMA_YUV_444) {
    pixelFormat = avifPixelFormat::AVIF_PIXEL_FORMAT_YUV444;
  } else if (preferredChromaSubsampling == AvifChromaSubsampling::AVIF_CHROMA_YUV_400) {
    pixelFormat = avifPixelFormat::AVIF_PIXEL_FORMAT_YUV400;
  }
  avif::ImagePtr image(avifImageCreate(info.width, info.height, 8, pixelFormat));

  if (image.get() == nullptr) {
    std::string str = "Can't create image for encoding";
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  uint32_t stride = info.width * 4;
  aligned_uint8_vector imageStore(stride * info.height);
  if (info.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
    coder::UnassociateRgba8(sourceData.data(), (int) info.stride,
                            imageStore.data(), stride, (int) info.width, (int) info.height);
  } else if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
    coder::Rgb565ToUnsigned8(reinterpret_cast<uint16_t *>(sourceData.data()), (int) info.stride,
                             imageStore.data(), stride, (int) info.width, (int) info.height, 255);
  } else if (info.format == ANDROID_BITMAP_FORMAT_RGBA_1010102) {
    coder::RGBA1010102ToUnsigned(reinterpret_cast<uint8_t *>(sourceData.data()),
                                 (uint32_t) info.stride,
                                 reinterpret_cast<uint8_t *>(imageStore.data()), stride,
                                 (uint32_t) info.width, (uint32_t) info.height, 8);
  } else if (info.format == ANDROID_BITMAP_FORMAT_RGBA_F16) {
    coder::RGBAF16BitToNBitU8(reinterpret_cast<const uint16_t *>(sourceData.data()),
                              (int) info.stride,
                              reinterpret_cast<uint8_t *>(imageStore.data()), stride,
                              (int) info.width,
                              (int) info.height, 8, false);
  }

  bool hasAlpha;
  switch (surface) {
    case RGB: {
      hasAlpha = false;
    }
      break;
    case RGBA: {
      hasAlpha = true;
    }
      break;
    default:hasAlpha = isImageHasAlpha(imageStore.data(), stride, info.width, info.height);
  }
  image->imageOwnsAlphaPlane = hasAlpha;

  auto result = avifImageAllocatePlanes(image.get(),
                                        hasAlpha ? avifPlanesFlag::AVIF_PLANES_ALL
                                                 : avifPlanesFlag::AVIF_PLANES_YUV);

  if (result != AVIF_RESULT_OK) {
    std::string str = "Can't allocate planes";
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  if (hasAlpha) {
    uint32_t aStride = image->alphaRowBytes;
    uint8_t *aPlane = image->alphaPlane;
    if (aPlane == nullptr) {
      std::string str = "Can't add A plane to an image";
      throwException(env, str);
      return static_cast<jbyteArray>(nullptr);
    }

    for (uint32_t y = 0; y < info.height; ++y) {
      auto vSrc = reinterpret_cast<uint8_t *>(imageStore.data()) + y * stride;
      auto vDst = reinterpret_cast<uint8_t *>(aPlane) + y * aStride;
      for (uint32_t x = 0; x < info.width; ++x) {
        vDst[0] = vSrc[3];
        vSrc += 4;
        vDst += 1;
      }
    }

  }

  uint32_t yStride = image->yuvRowBytes[0];
  uint8_t *yPlane = image->yuvPlanes[0];
  if (yPlane == nullptr) {
    std::string str = "Can't add Y plane to an image";
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  uint32_t uStride = 0;
  uint8_t *uPlane = nullptr;

  uint32_t vStride = 0;
  uint8_t *vPlane = nullptr;

  if (pixelFormat != AVIF_PIXEL_FORMAT_YUV400) {
    uStride = image->yuvRowBytes[1];
    uPlane = image->yuvPlanes[1];
    if (uPlane == nullptr) {
      std::string str = "Can't add U plane to an image";
      throwException(env, str);
      return static_cast<jbyteArray>(nullptr);
    }

    vStride = image->yuvRowBytes[2];
    vPlane = image->yuvPlanes[2];
    if (vPlane == nullptr) {
      std::string str = "Can't add V plane to an image";
      throwException(env, str);
      return static_cast<jbyteArray>(nullptr);
    }
  }

  std::vector<uint8_t> iccProfile(0);

  YuvMatrix matrix = YuvMatrix::Bt709;

  avifTransferCharacteristics transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_BT709;
  avifMatrixCoefficients matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT709;
  avifColorPrimaries colorPrimaries = AVIF_COLOR_PRIMARIES_BT709;
  avifRange yuvRange = AVIF_RANGE_LIMITED;

  bool nclxResult = coder::colorProfileFromDataSpaceAvif(dataSpace,
                                                         transferCharacteristics,
                                                         matrixCoefficients,
                                                         colorPrimaries,
                                                         yuvRange,
                                                         iccProfile,
                                                         matrix);

  if (pixelFormat == AVIF_PIXEL_FORMAT_YUV420) {
    RgbaToYuv420(imageStore.data(),
                 stride,
                 yPlane,
                 yStride,
                 uPlane,
                 uStride,
                 vPlane,
                 vStride,
                 info.width,
                 info.height,
                 yuvRange == AVIF_RANGE_FULL ? YuvRange::Full : YuvRange::Tv,
                 matrix);
  } else if (pixelFormat == AVIF_PIXEL_FORMAT_YUV422) {
    RgbaToYuv422(imageStore.data(),
                 stride,
                 yPlane,
                 yStride,
                 uPlane,
                 uStride,
                 vPlane,
                 vStride,
                 info.width,
                 info.height,
                 yuvRange == AVIF_RANGE_FULL ? YuvRange::Full : YuvRange::Tv,
                 matrix);
  } else if (pixelFormat == AVIF_PIXEL_FORMAT_YUV444) {
    RgbaToYuv444(imageStore.data(),
                 stride,
                 yPlane,
                 yStride,
                 uPlane,
                 uStride,
                 vPlane,
                 vStride,
                 info.width,
                 info.height,
                 yuvRange == AVIF_RANGE_FULL ? YuvRange::Full : YuvRange::Tv,
                 matrix);
  } else if (pixelFormat == AVIF_PIXEL_FORMAT_YUV400) {
    RgbaToYuv400(imageStore.data(),
                 stride,
                 yPlane,
                 yStride,
                 info.width,
                 info.height,
                 yuvRange == AVIF_RANGE_FULL ? YuvRange::Full : YuvRange::Tv,
                 matrix);
  }

  if (nclxResult) {
    if (iccProfile.empty()) {
      image->matrixCoefficients = matrixCoefficients;
      image->colorPrimaries = colorPrimaries;
      image->transferCharacteristics = transferCharacteristics;
      image->yuvRange = yuvRange;
    } else {
      result = avifImageSetProfileICC(image.get(), iccProfile.data(), iccProfile.size());
      if (result != AVIF_RESULT_OK) {
        std::string str = "Can't set required color profile";
        throwException(env, str);
        return static_cast<jbyteArray>(nullptr);
      }
    }
  }

  result = avifEncoderAddImage(encoder.get(), image.get(), 0, AVIF_ADD_IMAGE_FLAG_SINGLE);
  [[maybe_unused]] auto vrelease = image.release();
  if (result != AVIF_RESULT_OK) {
    [[maybe_unused]] auto erelease = encoder.release();
    std::string str = "Can't add an image";
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  avifRWData data = AVIF_DATA_EMPTY;
  result = avifEncoderFinish(encoder.get(), &data);
  if (result != AVIF_RESULT_OK) {
    [[maybe_unused]] auto erelease = encoder.release();
    std::string str = "Can't encode an image";
    throwException(env, str);
    return static_cast<jbyteArray>(nullptr);
  }

  [[maybe_unused]] auto erelease = encoder.release();

  jbyteArray byteArray = env->NewByteArray((jsize) data.size);
  char *memBuf = (char *) ((void *) data.data);
  env->SetByteArrayRegion(byteArray, 0, (jint) data.size,
                          reinterpret_cast<const jbyte *>(memBuf));

  avifRWDataFree(&data);
  return byteArray;
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_encodeAvifImpl(JNIEnv *env,
                                                                jobject thiz,
                                                                jobject bitmap,
                                                                jint quality,
                                                                jint dataSpace,
                                                                jint qualityMode,
                                                                jint surfaceMode,
                                                                jint speed,
                                                                jint chromaSubsampling) {
  try {
    AvifEncodingSurface surface = AvifEncodingSurface::AUTO;
    if (surfaceMode == 1) {
      surface = AvifEncodingSurface::RGB;
    } else if (surfaceMode == 2) {
      surface = AvifEncodingSurface::RGBA;
    }
    AvifChromaSubsampling mChromaSubsampling = AvifChromaSubsampling::AVIF_CHROMA_AUTO;
    if (chromaSubsampling == 1) {
      mChromaSubsampling = AvifChromaSubsampling::AVIF_CHROMA_YUV_420;
    } else if (chromaSubsampling == 2) {
      mChromaSubsampling = AvifChromaSubsampling::AVIF_CHROMA_YUV_422;
    } else if (chromaSubsampling == 3) {
      mChromaSubsampling = AvifChromaSubsampling::AVIF_CHROMA_YUV_444;
    } else if (chromaSubsampling == 4) {
      mChromaSubsampling = AvifChromaSubsampling::AVIF_CHROMA_YUV_400;
    }
    return encodeBitmapAvif(env,
                            thiz,
                            bitmap,
                            quality,
                            dataSpace,
                            static_cast<AvifQualityMode>(qualityMode),
                            surface, speed,
                            mChromaSubsampling);
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to encode this image";
    throwException(env, exception);
    return static_cast<jbyteArray>(nullptr);
  }
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_encodeHeicImpl(JNIEnv *env,
                                                                jobject thiz,
                                                                jobject bitmap,
                                                                jint quality,
                                                                jint dataSpace,
                                                                jint qualityMode,
                                                                jint x265Preset, jint crf) {
  try {
    std::string preset = "superfast";
    if (x265Preset == 0) {
      preset = "placebo";
    } else if (x265Preset == 1) {
      preset = "veryslow";
    } else if (x265Preset == 2) {
      preset = "slower";
    } else if (x265Preset == 3) {
      preset = "slow";
    } else if (x265Preset == 4) {
      preset = "medium";
    } else if (x265Preset == 5) {
      preset = "fast";
    } else if (x265Preset == 6) {
      preset = "faster";
    } else if (x265Preset == 7) {
      preset = "veryfast";
    } else if (x265Preset == 8) {
      preset = "superfast";
    } else if (x265Preset == 9) {
      preset = "ultrafast";
    }

    return encodeBitmapHevc(env,
                            thiz,
                            bitmap,
                            quality,
                            dataSpace,
                            qualityMode == 2,
                            preset, crf);
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to encode this image";
    throwException(env, exception);
    return static_cast<jbyteArray>(nullptr);
  }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_isHeifImageImpl(JNIEnv *env, jobject thiz,
                                                                 jbyteArray byte_array) {
  try {
    auto totalLength = env->GetArrayLength(byte_array);
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(byte_array, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));
    auto cMime = heif_get_file_mime_type(reinterpret_cast<const uint8_t *>(srcBuffer.data()),
                                         totalLength);
    std::string mime(cMime);
    return mime == "image/heic" || mime == "image/heif" ||
        mime == "image/heic-sequence" || mime == "image/heif-sequence";
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to check this image";
    throwException(env, exception);
    return false;
  }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_isAvifImageImpl(JNIEnv *env, jobject thiz,
                                                                 jbyteArray byte_array) {
  try {
    auto totalLength = env->GetArrayLength(byte_array);
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(byte_array, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));
    auto cMime = heif_get_file_mime_type(reinterpret_cast<const uint8_t *>(srcBuffer.data()),
                                         totalLength);
    std::string mime(cMime);
    return mime == "image/avif" || mime == "image/avif-sequence";
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to check this image";
    throwException(env, exception);
    return false;
  }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_isSupportedImageImpl(JNIEnv *env, jobject thiz,
                                                                      jbyteArray byte_array) {
  try {
    auto totalLength = env->GetArrayLength(byte_array);
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(byte_array, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));
    auto cMime = heif_get_file_mime_type(reinterpret_cast<const uint8_t *>(srcBuffer.data()),
                                         totalLength);
    if (!cMime) {
      return false;
    }
    std::string mime(cMime);
    return mime == "image/heic" || mime == "image/heif" ||
        mime == "image/heic-sequence" || mime == "image/heif-sequence" ||
        mime == "image/avif" || mime == "image/avif-sequence";
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to check this image";
    throwException(env, exception);
    return false;
  }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_getSizeImpl(JNIEnv *env, jobject thiz,
                                                             jbyteArray byteArray) {
  try {
    std::shared_ptr<heif_context> ctx(heif_context_alloc(),
                                      [](heif_context *c) { heif_context_free(c); });
    if (!ctx) {
      std::string exception = "Acquiring an image from buffer has failed";
      throwException(env, exception);
      return static_cast<jobject>(nullptr);
    }
    auto totalLength = env->GetArrayLength(byteArray);
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(byteArray, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));

    auto cMime = heif_get_file_mime_type(reinterpret_cast<const uint8_t *>(srcBuffer.data()),
                                         totalLength);
    if (!cMime) {
      std::string exception = "Acquiring an image from buffer has failed";
      throwException(env, exception);
      return static_cast<jobject>(nullptr);
    }
    std::string mime(cMime);
    if (mime == "image/avif" || mime == "image/avif-sequence") {
      AvifImageSize size = AvifDecoderController::getImageSize(srcBuffer.data(), srcBuffer.size());
      jclass sizeClass = env->FindClass("android/util/Size");
      jmethodID methodID = env->GetMethodID(sizeClass, "<init>", "(II)V");
      auto sizeObject = env->NewObject(sizeClass, methodID, size.width, size.height);
      return sizeObject;
    }

    auto result = heif_context_read_from_memory_without_copy(ctx.get(), srcBuffer.data(),
                                                             totalLength,
                                                             nullptr);
    if (result.code != heif_error_Ok) {
      std::string exception = "Reading an file buffer has failed";
      throwException(env, exception);
      return static_cast<jobject>(nullptr);
    }

    heif_image_handle *handle;
    result = heif_context_get_primary_image_handle(ctx.get(), &handle);
    if (result.code != heif_error_Ok) {
      std::string exception = "Acquiring an image from buffer has failed";
      throwException(env, exception);
      return static_cast<jobject>(nullptr);
    }
    int bitDepth = heif_image_handle_get_chroma_bits_per_pixel(handle);
    if (bitDepth < 0) {
      heif_image_handle_release(handle);
      throwBitDepthException(env);
      return static_cast<jobject>(nullptr);
    }
    auto width = heif_image_handle_get_width(handle);
    auto height = heif_image_handle_get_height(handle);
    heif_image_handle_release(handle);

    jclass sizeClass = env->FindClass("android/util/Size");
    jmethodID methodID = env->GetMethodID(sizeClass, "<init>", "(II)V");
    auto sizeObject = env->NewObject(sizeClass, methodID, width, height);
    return sizeObject;
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to load size of this image";
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  } catch (std::runtime_error &err) {
    std::string exception(err.what());
    throwException(env, exception);
    return static_cast<jobject>(nullptr);
  }
}


extern "C"
JNIEXPORT jboolean JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_isSupportedImageImplBB(JNIEnv *env, jobject thiz,
                                                                        jobject byteBuffer) {
  try {
    auto bufferAddress = reinterpret_cast<uint8_t *>(env->GetDirectBufferAddress(byteBuffer));
    int length = (int) env->GetDirectBufferCapacity(byteBuffer);
    if (!bufferAddress || length <= 0) {
      std::string errorString = "Only direct byte buffers are supported";
      throwException(env, errorString);
      return (jboolean) false;
    }
    std::vector<uint8_t> srcBuffer(length);
    std::copy(bufferAddress, bufferAddress + length, srcBuffer.begin());
    auto cMime = heif_get_file_mime_type(reinterpret_cast<const uint8_t *>(srcBuffer.data()),
                                         (int) srcBuffer.size());
    std::string mime(cMime);
    return mime == "image/heic" || mime == "image/heif" ||
        mime == "image/heic-sequence" || mime == "image/heif-sequence" ||
        mime == "image/avif" || mime == "image/avif-sequence";
  } catch (std::bad_alloc &err) {
    std::string exception = "Not enough memory to check this image";
    throwException(env, exception);
    return false;
  }
}