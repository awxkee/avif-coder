#include <jni.h>
#include <string>
#include "libheif/heif.h"
#include "libyuv/libyuv.h"
#include "android/bitmap.h"
#include "libyuv/convert_argb.h"
#include <vector>
#include <float.h>
#include <arm_fp16.h>
#include "jni_exception.h"
#include "scaler.h"
#include <android/log.h>
#include <android/data_space.h>
#include <sys/system_properties.h>
#include "icc/lcms2.h"
#include "rgba_to_bgra.h"
#include "colorspace.h"
#include <math.h>
#include <limits>
#include "attenuate_alpha.h"

struct AvifMemEncoder {
    std::vector<char> buffer;
};

int androidOSVersion() {
    return android_get_device_api_level();
}

struct heif_error writeHeifData(struct heif_context *ctx,
                                const void *data,
                                size_t size,
                                void *userdata) {
    auto *p = (struct AvifMemEncoder *) userdata;
    p->buffer.insert(p->buffer.end(), (char *) data, (char *) ((char *) data + size));

    struct heif_error
            error_ok;
    error_ok.code = heif_error_Ok;
    error_ok.subcode = heif_suberror_Unspecified;
    error_ok.message = "ok";
    return (error_ok);
}

jbyteArray encodeBitmap(JNIEnv *env, jobject thiz,
                        jobject bitmap, heif_compression_format heifCompressionFormat,
                        int quality) {
    std::shared_ptr<heif_context> ctx(heif_context_alloc(),
                                      [](heif_context *c) { heif_context_free(c); });
    if (!ctx) {
        throwCoderCreationException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    heif_encoder *mEncoder;
    auto result = heif_context_get_encoder_for_format(ctx.get(), heifCompressionFormat, &mEncoder);
    if (result.code != heif_error_Ok) {
        throwCantEncodeImageException(env, result.message);
        return static_cast<jbyteArray>(nullptr);
    }
    std::shared_ptr<heif_encoder> encoder(mEncoder,
                                          [](heif_encoder *he) { heif_encoder_release(he); });
    if (quality < 100) {
        heif_encoder_set_lossy_quality(encoder.get(), quality);
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

    heif_image *image;
    heif_chroma chroma = heif_chroma_interleaved_RGBA;
    if (info.format == ANDROID_BITMAP_FORMAT_RGBA_F16 &&
        heifCompressionFormat == heif_compression_AV1) {
        chroma = heif_chroma_interleaved_RRGGBBAA_LE;
    }
    result = heif_image_create((int) info.width, (int) info.height, heif_colorspace_RGB,
                               chroma, &image);
    if (result.code != heif_error_Ok) {
        AndroidBitmap_unlockPixels(env, bitmap);
        throwCantEncodeImageException(env, result.message);
        return static_cast<jbyteArray>(nullptr);
    }

    int bitDepth = 8;
    if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        bitDepth = 8;
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGBA_F16 &&
               heifCompressionFormat == heif_compression_AV1) {
        bitDepth = 10;
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGBA_1010102 &&
               heifCompressionFormat == heif_compression_AV1) {
        bitDepth = 10;
    }

    result = heif_image_add_plane(image, heif_channel_interleaved, (int) info.width,
                                  (int) info.height, bitDepth);
    if (result.code != heif_error_Ok) {
        AndroidBitmap_unlockPixels(env, bitmap);
        throwCantEncodeImageException(env, result.message);
        return static_cast<jbyteArray>(nullptr);
    }
    int stride;
    uint8_t *imgData = heif_image_get_plane(image, heif_channel_interleaved, &stride);
    if (info.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
        libyuv::ARGBCopy(reinterpret_cast<const uint8_t *>(addr), (int) info.stride, imgData,
                         stride, (int) info.width, (int) info.height);
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        libyuv::RGB565ToARGB(reinterpret_cast<const uint8_t *>(addr), (int) info.stride, imgData,
                             stride, (int) info.width, (int) info.height);
        libyuv::ARGBToABGR(imgData, stride, imgData, stride, (int) info.width, (int) info.height);
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGBA_1010102) {
        if (heifCompressionFormat == heif_compression_HEVC) {
            auto dstY = (char *) imgData;
            auto srcY = (char *) addr;
            for (int y = 0; y < info.height; ++y) {
                memcpy(dstY, srcY, info.width * 4 * sizeof(uint32_t));
                srcY += info.width * sizeof(uint64_t);
                dstY += stride;
            }
        } else {
            libyuv::AR30ToABGR(static_cast<const uint8_t *>(addr), (int) info.stride, imgData,
                               stride, (int) info.width,
                               (int) info.height);
        }
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGBA_F16) {
        if (heifCompressionFormat == heif_compression_AV1) {
            std::shared_ptr<char> dstARGB(
                    static_cast<char *>(malloc(info.width * info.height * 4 * sizeof(uint16_t))),
                    [](char *f) { free(f); });
            auto *srcData = static_cast<float16_t *>(addr);
            uint16_t tmpR;
            uint16_t tmpG;
            uint16_t tmpB;
            uint16_t tmpA;
            auto *data64Ptr = reinterpret_cast<uint64_t *>(dstARGB.get());
            const float maxColors = (float) pow(2.0, bitDepth) - 1;
            for (int i = 0, k = 0; i < std::min(info.stride * info.height,
                                                info.width * info.height * 4); i += 4, k += 1) {
                tmpR = (uint16_t) (srcData[i] * maxColors);
                tmpG = (uint16_t) (srcData[i + 1] * maxColors);
                tmpB = (uint16_t) (srcData[i + 2] * maxColors);
                tmpA = (uint16_t) (srcData[i + 3] * maxColors);
                uint64_t color =
                        ((uint64_t) tmpA & 0xffff) << 48 | ((uint64_t) tmpB & 0xffff) << 32 |
                        ((uint64_t) tmpG & 0xffff) << 16 | ((uint64_t) tmpR & 0xffff);
                data64Ptr[k] = color;
            }
            auto *dataPtr = reinterpret_cast<void *>(dstARGB.get());
            auto srcY = (char *) dataPtr;
            auto dstY = (char *) imgData;
            const auto sourceStride = info.width * 4 * sizeof(uint16_t);
            for (int y = 0; y < info.height; ++y) {
                memcpy(dstY, srcY, sourceStride);
                srcY += sourceStride;
                dstY += stride;
            }
            dstARGB.reset();
        } else {
            std::shared_ptr<char> dstARGB(
                    static_cast<char *>(malloc(info.width * info.height * 4 * sizeof(uint8_t))),
                    [](char *f) { free(f); });
            auto *srcData = static_cast<float16_t *>(addr);
            char tmpR;
            char tmpG;
            char tmpB;
            char tmpA;
            auto *data64Ptr = reinterpret_cast<uint32_t *>(dstARGB.get());
            const float maxColors = (float) pow(2.0, 8) - 1;
            for (int i = 0, k = 0; i < std::min(info.stride * info.height,
                                                info.width * info.height * 4); i += 4, k += 1) {
                tmpR = (char) (srcData[i] * maxColors);
                tmpG = (char) (srcData[i + 1] * maxColors);
                tmpB = (char) (srcData[i + 2] * maxColors);
                tmpA = (char) (srcData[i + 3] * maxColors);
                uint32_t color =
                        ((uint32_t) tmpA & 0xff) << 24 | ((uint32_t) tmpB & 0xff) << 16 |
                        ((uint32_t) tmpG & 0xff) << 8 | ((uint32_t) tmpR & 0xff);
                data64Ptr[k] = color;
            }
            auto *dataPtr = reinterpret_cast<void *>(dstARGB.get());
            auto srcY = (char *) dataPtr;
            auto dstY = (char *) imgData;
            const auto sourceStride = info.width * 4 * sizeof(uint8_t);
            for (int y = 0; y < info.height; ++y) {
                memcpy(dstY, srcY, sourceStride);
                srcY += sourceStride;
                dstY += stride;
            }
            dstARGB.reset();
        }
    }
    AndroidBitmap_unlockPixels(env, bitmap);

    heif_image_handle *handle;
    std::shared_ptr<heif_encoding_options> options(heif_encoding_options_alloc(),
                                                   [](heif_encoding_options *o) {
                                                       heif_encoding_options_free(o);
                                                   });
    options->version = 5;
    options->image_orientation = heif_orientation_normal;
    result = heif_context_encode_image(ctx.get(), image, encoder.get(), options.get(), &handle);
    options.reset();
    if (handle && result.code == heif_error_Ok) {
        heif_context_set_primary_image(ctx.get(), handle);
        heif_image_handle_release(handle);
    }
    heif_image_release(image);
    if (result.code != heif_error_Ok) {
        throwCantEncodeImageException(env, result.message);
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
        throwCantEncodeImageException(env, result.message);
        return static_cast<jbyteArray>(nullptr);
    }

    jbyteArray byteArray = env->NewByteArray((jsize) memEncoder.buffer.size());
    char *memBuf = (char *) ((void *) memEncoder.buffer.data());
    env->SetByteArrayRegion(byteArray, 0, (jint) memEncoder.buffer.size(),
                            reinterpret_cast<const jbyte *>(memBuf));
    return byteArray;
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_encodeAvifImpl(JNIEnv *env, jobject thiz,
                                                                jobject bitmap, jint quality) {
    return encodeBitmap(env, thiz, bitmap, heif_compression_AV1, quality);
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_encodeHeicImpl(JNIEnv *env, jobject thiz,
                                                                jobject bitmap, jint quality) {
    return encodeBitmap(env, thiz, bitmap, heif_compression_HEVC, quality);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_isHeifImageImpl(JNIEnv *env, jobject thiz,
                                                                 jbyteArray byte_array) {
    auto totalLength = env->GetArrayLength(byte_array);
    std::shared_ptr<void> srcBuffer(static_cast<char *>(malloc(totalLength)),
                                    [](void *b) { free(b); });
    env->GetByteArrayRegion(byte_array, 0, totalLength, reinterpret_cast<jbyte *>(srcBuffer.get()));
    auto mime = heif_get_file_mime_type(reinterpret_cast<const uint8_t *>(srcBuffer.get()),
                                        totalLength);
    return strcmp(mime, "image/heic") == 0 || strcmp(mime, "image/heif") == 0 ||
           strcmp(mime, "image/heic-sequence") == 0 || strcmp(mime, "image/heif-sequence") == 0;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_isAvifImageImpl(JNIEnv *env, jobject thiz,
                                                                 jbyteArray byte_array) {
    auto totalLength = env->GetArrayLength(byte_array);
    std::shared_ptr<void> srcBuffer(static_cast<char *>(malloc(totalLength)),
                                    [](void *b) { free(b); });
    env->GetByteArrayRegion(byte_array, 0, totalLength, reinterpret_cast<jbyte *>(srcBuffer.get()));
    auto mime = heif_get_file_mime_type(reinterpret_cast<const uint8_t *>(srcBuffer.get()),
                                        totalLength);
    return strcmp(mime, "image/avif") == 0 || strcmp(mime, "image/avif-sequence") == 0;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_isSupportedImageImpl(JNIEnv *env, jobject thiz,
                                                                      jbyteArray byte_array) {
    auto totalLength = env->GetArrayLength(byte_array);
    std::shared_ptr<void> srcBuffer(static_cast<char *>(malloc(totalLength)),
                                    [](void *b) { free(b); });
    env->GetByteArrayRegion(byte_array, 0, totalLength, reinterpret_cast<jbyte *>(srcBuffer.get()));
    auto mime = heif_get_file_mime_type(reinterpret_cast<const uint8_t *>(srcBuffer.get()),
                                        totalLength);
    return strcmp(mime, "image/heic") == 0 || strcmp(mime, "image/heif") == 0 ||
           strcmp(mime, "image/heic-sequence") == 0 || strcmp(mime, "image/heif-sequence") == 0 ||
           strcmp(mime, "image/avif") == 0 || strcmp(mime, "image/avif-sequence") == 0;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_getSizeImpl(JNIEnv *env, jobject thiz,
                                                             jbyteArray byte_array) {
    std::shared_ptr<heif_context> ctx(heif_context_alloc(),
                                      [](heif_context *c) { heif_context_free(c); });
    if (!ctx) {
        return static_cast<jobject>(nullptr);
    }
    auto totalLength = env->GetArrayLength(byte_array);
    std::shared_ptr<void> srcBuffer(static_cast<char *>(malloc(totalLength)),
                                    [](void *b) { free(b); });
    env->GetByteArrayRegion(byte_array, 0, totalLength, reinterpret_cast<jbyte *>(srcBuffer.get()));
    auto result = heif_context_read_from_memory_without_copy(ctx.get(), srcBuffer.get(),
                                                             totalLength,
                                                             nullptr);
    if (result.code != heif_error_Ok) {
        throwCannotReadFileException(env);
        return static_cast<jobject>(nullptr);
    }

    heif_image_handle *handle;
    result = heif_context_get_primary_image_handle(ctx.get(), &handle);
    if (result.code != heif_error_Ok) {
        throwCannotReadFileException(env);
        return static_cast<jobject>(nullptr);
    }
    int bitDepth = heif_image_handle_get_chroma_bits_per_pixel(handle);
    if (bitDepth < 0) {
        heif_image_handle_release(handle);
        throwBitDepthException(env);
        return static_cast<jobject>(nullptr);
    }
    heif_image *img;
    std::shared_ptr<heif_decoding_options> options(heif_decoding_options_alloc(),
                                                   [](heif_decoding_options *deo) {
                                                       heif_decoding_options_free(deo);
                                                   });
    options->convert_hdr_to_8bit = true;
    result = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_interleaved_RGBA,
                               options.get());
    options.reset();
    if (result.code != heif_error_Ok) {
        heif_image_handle_release(handle);
        throwCantDecodeImageException(env);
        return static_cast<jobject>(nullptr);
    }

    auto width = heif_image_get_primary_width(img);
    auto height = heif_image_get_primary_height(img);

    heif_image_release(img);
    heif_image_handle_release(handle);

    jclass sizeClass = env->FindClass("android/util/Size");
    jmethodID methodID = env->GetMethodID(sizeClass, "<init>", "(II)V");
    auto sizeObject = env->NewObject(sizeClass, methodID, width, height);
    return sizeObject;
}

uint16_t convert12to16(uint16_t value12) {
    return (value12 << (16 - 12)); // Left-shift the 12-bit value to fill 16 bits.
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_decodeImpl(JNIEnv *env, jobject thiz,
                                                            jbyteArray byte_array, jint scaledWidth,
                                                            jint scaledHeight) {
    std::shared_ptr<heif_context> ctx(heif_context_alloc(),
                                      [](heif_context *c) { heif_context_free(c); });
    if (!ctx) {
        throwCoderCreationException(env);
        return static_cast<jobject>(nullptr);
    }
    auto totalLength = env->GetArrayLength(byte_array);
    std::shared_ptr<void> srcBuffer(static_cast<char *>(malloc(totalLength)),
                                    [](void *b) { free(b); });
    env->GetByteArrayRegion(byte_array, 0, totalLength, reinterpret_cast<jbyte *>(srcBuffer.get()));
    auto result = heif_context_read_from_memory_without_copy(ctx.get(), srcBuffer.get(),
                                                             totalLength,
                                                             nullptr);
    if (result.code != heif_error_Ok) {
        throwCannotReadFileException(env);
        return static_cast<jobject>(nullptr);
    }

    heif_image_handle *handle;
    result = heif_context_get_primary_image_handle(ctx.get(), &handle);
    if (result.code != heif_error_Ok) {
        throwCannotReadFileException(env);
        return static_cast<jobject>(nullptr);
    }

    int bitDepth = heif_image_handle_get_chroma_bits_per_pixel(handle);
    bool useBitmapHalf16Floats = false;
    if (bitDepth < 0) {
        heif_image_handle_release(handle);
        throwBitDepthException(env);
        return static_cast<jobject>(nullptr);
    }

    int osVersion = androidOSVersion();

    if (bitDepth > 8 && osVersion >= 26) {
        useBitmapHalf16Floats = true;
    }
    heif_image *img;
    std::shared_ptr<heif_decoding_options> options(heif_decoding_options_alloc(),
                                                   [](heif_decoding_options *deo) {
                                                       heif_decoding_options_free(deo);
                                                   });
    options->convert_hdr_to_8bit = false;
    options->ignore_transformations = false;
    result = heif_decode_image(handle, &img, heif_colorspace_RGB,
                               useBitmapHalf16Floats ? heif_chroma_interleaved_RRGGBBAA_LE
                                                     : heif_chroma_interleaved_RGBA,
                               options.get());
    options.reset();

    if (result.code != heif_error_Ok) {
        heif_image_handle_release(handle);
        throwCantDecodeImageException(env);
        return static_cast<jobject>(nullptr);
    }

    /*
     *     SRGB,
        LINEAR_SRGB,
        EXTENDED_SRGB,
        LINEAR_EXTENDED_SRGB,
        BT709,
        BT2020,
        DCI_P3,
        DISPLAY_P3,
        NTSC_1953,
        SMPTE_C,
        ADOBE_RGB,
        PRO_PHOTO_RGB,
        ACES,
        ACESCG,
        CIE_XYZ,
        CIE_LAB,
        BT2020_HLG,
        BT2020_PQ;
     */

    std::vector<uint8_t> profile;
    bool hasICC = false;

    heif_color_profile_nclx *colorProfileNclx = nullptr;
    auto type = heif_image_get_color_profile_type(img);

    int dataSpace = -1;
    uint8_t colorSpaceRequiredVersion = 29;
    const char *colorSpaceName = nullptr;

    auto nclxColorProfile = heif_image_handle_get_nclx_color_profile(handle, &colorProfileNclx);
    if (nclxColorProfile.code == heif_error_Ok) {
        if (colorProfileNclx && colorProfileNclx->color_primaries != 0 &&
            colorProfileNclx->transfer_characteristics != 0) {
            auto transfer = colorProfileNclx->transfer_characteristics;
            auto colorPrimaries = colorProfileNclx->color_primaries;
            if (colorPrimaries == heif_color_primaries_ITU_R_BT_2020_2_and_2100_0 &&
                transfer == heif_transfer_characteristic_ITU_R_BT_2100_0_PQ) {
                dataSpace = (int) ADataSpace::ADATASPACE_BT2020_ITU_PQ;
                colorSpaceName = "BT2020_PQ";
                colorSpaceRequiredVersion = 34;
            } else if (colorPrimaries == heif_color_primaries_ITU_R_BT_709_5 &&
                       transfer == heif_transfer_characteristic_linear) {
                colorSpaceName = "LINEAR_SRGB";
            } else if (colorPrimaries == heif_color_primaries_ITU_R_BT_709_5 &&
                       transfer == heif_transfer_characteristic_ITU_R_BT_709_5) {
                colorSpaceName = "BT709";
            } else if (colorPrimaries == heif_color_primaries_ITU_R_BT_2020_2_and_2100_0 &&
                       transfer == heif_transfer_characteristic_linear) {
                hasICC = true;
                profile.resize(sizeof(linearExtendedBT2020));
                std::copy(&linearExtendedBT2020[0],
                          &linearExtendedBT2020[0] + sizeof(linearExtendedBT2020), profile.data());
            } else if (colorPrimaries == heif_color_primaries_ITU_R_BT_2020_2_and_2100_0 &&
                       (transfer == heif_transfer_characteristic_ITU_R_BT_2020_2_10bit ||
                        transfer == heif_transfer_characteristic_ITU_R_BT_2020_2_12bit)) {
                colorSpaceName = "BT2020";
            } else if (colorPrimaries == heif_color_primaries_SMPTE_EG_432_1 &&
                       transfer == heif_transfer_characteristic_ITU_R_BT_2100_0_HLG) {
                hasICC = true;
                profile.resize(sizeof(displayP3_HLG));
                std::copy(&displayP3_HLG[0],
                          &displayP3_HLG[0] + sizeof(displayP3_HLG), profile.data());
            } else if (colorPrimaries == heif_color_primaries_SMPTE_EG_432_1 &&
                       transfer == heif_transfer_characteristic_IEC_61966_2_1) {
                colorSpaceName = "DISPLAY_P3";
            } else if (colorPrimaries == heif_color_primaries_ITU_R_BT_2020_2_and_2100_0) {
                colorSpaceName = "BT2020";
            }
        }
    } else if (type == heif_color_profile_type_prof || type == heif_color_profile_type_rICC) {
        auto profileSize = heif_image_get_raw_color_profile_size(img);
        if (profileSize > 0) {
            profile.resize(profileSize);
            auto iccStatus = heif_image_get_raw_color_profile(img, profile.data());
            if (iccStatus.code == heif_error_Ok) {
                hasICC = true;
            } else {
                if (iccStatus.message) {
                    __android_log_print(ANDROID_LOG_ERROR, "AVIF",
                                        "ICC profile retrieving failed with: %s",
                                        iccStatus.message);
                } else {
                    __android_log_print(ANDROID_LOG_ERROR, "AVIF",
                                        "ICC profile retrieving failed with unknown error");
                }
            }
        }
    }

    bool alphaPremultiplied = true;
    if (heif_image_handle_has_alpha_channel(handle)) {
        alphaPremultiplied = heif_image_handle_is_premultiplied_alpha(handle) != 0;
    } else {
        alphaPremultiplied = false;
    }

    if (scaledHeight > 0 && scaledWidth > 0) {
        heif_image *scaledImg;
        result = heif_image_scale_image(img, &scaledImg, scaledWidth, scaledHeight, nullptr);
        if (result.code != heif_error_Ok) {
            heif_image_release(img);
            heif_image_handle_release(handle);
            throwInvalidScale(env, result.message);
            return static_cast<jobject>(nullptr);
        }
        heif_image_release(img);
        img = scaledImg;
    }

    int stride;

    const uint8_t *data = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);

    auto imageWidth = heif_image_get_width(img, heif_channel_interleaved);
    auto imageHeight = heif_image_get_height(img, heif_channel_interleaved);

    std::shared_ptr<char> dstARGB(static_cast<char *>(malloc(stride * imageHeight)),
                                  [](char *f) { free(f); });

    if (useBitmapHalf16Floats) {
        const float scale = 1.0f / float((1 << bitDepth) - 1);
        libyuv::HalfFloatPlane(reinterpret_cast<const uint16_t *>(data),
                               stride,
                               reinterpret_cast<uint16_t *>(dstARGB.get()),
                               stride,
                               scale,
                               imageWidth * 4,
                               imageHeight);

    } else {
        std::copy(data, data + stride * imageHeight, dstARGB.get());
    }

    heif_image_release(img);
    heif_image_handle_release(handle);

    if (hasICC) {
        convertUseDefinedColorSpace(dstARGB, stride, imageHeight, profile.data(), profile.size(),
                                    useBitmapHalf16Floats);
        colorSpaceName = "SRGB";
    } else if (colorSpaceName && strcmp(colorSpaceName, "BT2020_PQ") == 0 &&
               osVersion < colorSpaceRequiredVersion) {
        convertUseDefinedColorSpace(dstARGB, stride, imageHeight, &bt2020PQ[0],
                                    sizeof(bt2020PQ),
                                    useBitmapHalf16Floats);
        colorSpaceName = "SRGB";
        colorSpaceRequiredVersion = 29;
    } else if (colorSpaceName && strcmp(colorSpaceName, "BT2020") == 0 &&
               osVersion < colorSpaceRequiredVersion) {
        convertUseDefinedColorSpace(dstARGB, stride, imageHeight, &bt2020[0], sizeof(bt2020),
                                    useBitmapHalf16Floats);
        colorSpaceName = "SRGB";
    } else if (colorSpaceName && strcmp(colorSpaceName, "DISPLAY_P3") == 0 &&
               osVersion < colorSpaceRequiredVersion) {
        convertUseDefinedColorSpace(dstARGB, stride, imageHeight, &displayP3[0], sizeof(displayP3),
                                    useBitmapHalf16Floats);
        colorSpaceName = "SRGB";
    } else if (colorSpaceName && strcmp(colorSpaceName, "LINEAR_SRGB") == 0 &&
               osVersion < colorSpaceRequiredVersion) {
        convertUseDefinedColorSpace(dstARGB, stride, imageHeight, &linearSRGB[0],
                                    sizeof(linearSRGB), useBitmapHalf16Floats);
        colorSpaceName = "SRGB";
    } else if (colorSpaceName && strcmp(colorSpaceName, "BT709") == 0 &&
               osVersion < colorSpaceRequiredVersion) {
        convertUseDefinedColorSpace(dstARGB, stride, imageHeight, &bt709[0],
                                    sizeof(bt709), useBitmapHalf16Floats);
        colorSpaceName = "SRGB";
    }

    if (!alphaPremultiplied) {
        if (!useBitmapHalf16Floats) {
            libyuv::ARGBAttenuate(reinterpret_cast<uint8_t *>(dstARGB.get()), stride,
                                  reinterpret_cast<uint8_t *>(dstARGB.get()), stride, imageWidth,
                                  imageHeight);
        } else {
            // Premultiplying HDR images in half float currently not implemented
        }

    }

    jclass bitmapConfig = env->FindClass("android/graphics/Bitmap$Config");
    jfieldID rgba8888FieldID = env->GetStaticFieldID(bitmapConfig,
                                                     useBitmapHalf16Floats ? "RGBA_F16"
                                                                           : "ARGB_8888",
                                                     "Landroid/graphics/Bitmap$Config;");
    jobject rgba8888Obj = env->GetStaticObjectField(bitmapConfig, rgba8888FieldID);

    jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
    jmethodID createBitmapMethodID = env->GetStaticMethodID(bitmapClass, "createBitmap",
                                                            "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jobject bitmapObj = env->CallStaticObjectMethod(bitmapClass, createBitmapMethodID,
                                                    imageWidth, imageHeight, rgba8888Obj);

    void *addr;
    if (AndroidBitmap_lockPixels(env, bitmapObj, &addr) != 0) {
        throwPixelsException(env);
        return static_cast<jobject>(nullptr);
    }

    std::copy(dstARGB.get(), dstARGB.get() + stride * imageHeight, (char *) addr);

    if (AndroidBitmap_unlockPixels(env, bitmapObj) != 0) {
        throwPixelsException(env);
        return static_cast<jobject>(nullptr);
    }

    if ((dataSpace != -1 || colorSpaceName != nullptr) && osVersion >= 29) {
        if (osVersion >= colorSpaceRequiredVersion) {
            jclass cls = env->FindClass("android/graphics/ColorSpace");
            jmethodID staticMethodGetColorSpace = env->GetStaticMethodID(cls, "get",
                                                                         "(Landroid/graphics/ColorSpace$Named;)Landroid/graphics/ColorSpace;");
            jclass colorSpaceNameCls = env->FindClass("android/graphics/ColorSpace$Named");
            jfieldID colorSpaceNameFieldID = env->GetStaticFieldID(colorSpaceNameCls,
                                                                   colorSpaceName,
                                                                   "Landroid/graphics/ColorSpace$Named;");
            jobject colorSpaceNameObj = env->GetStaticObjectField(colorSpaceNameCls,
                                                                  colorSpaceNameFieldID);
            jobject colorSpace = env->CallStaticObjectMethod(cls, staticMethodGetColorSpace,
                                                             colorSpaceNameObj);
            jmethodID setColorSpaceMethod = env->GetMethodID(bitmapClass, "setColorSpace",
                                                             "(Landroid/graphics/ColorSpace;)V");
            if (setColorSpaceMethod) {
                env->CallVoidMethod(bitmapObj, setColorSpaceMethod, colorSpace);
            }
        }
    }

    return bitmapObj;
}
