//
// Created by Radzivon Bartoshyk on 15/09/2023.
//
#include <jni.h>
#include <string>
#include "libheif/heif.h"
#include "libyuv/libyuv.h"
#include "android/bitmap.h"
#include "libyuv/convert_argb.h"
#include <vector>
#include "JniException.h"
#include "scaler.h"
#include <android/log.h>
#include <android/data_space.h>
#include <sys/system_properties.h>
#include "icc/lcms2.h"
#include "colorspace/colorspace.h"
#include <algorithm>
#include <limits>
#include "imagebits/attenuate_alpha.h"
#include "imagebits/HalfFloats.h"
#include "imagebits/RgbaF16bitToNBitU16.h"
#include "imagebits/RgbaF16bitNBitU8.h"
#include "imagebits/Rgb1010102.h"
#include "colorspace/PerceptualQuantinizer.h"
#include "imagebits/CopyUnalignedRGBA.h"
#include "colorspace/HLG.h"
#include "Support.h"
#include "imagebits/Rgba8ToF16.h"
#include "imagebits/Rgb565.h"

extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_decodeImpl(JNIEnv *env, jobject thiz,
                                                            jbyteArray byte_array, jint scaledWidth,
                                                            jint scaledHeight,
                                                            jint javaColorspace) {
    auto preferredColorSpace = static_cast<PreferredColorConfig>(javaColorspace);
    if (!preferredColorSpace) {
        std::string errorString =
                "Invalid Color Config: " + std::to_string(javaColorspace) + " was passed";
        throwException(env, errorString);
        return static_cast<jbyteArray>(nullptr);
    }

    int osVersion = androidOSVersion();

    if (preferredColorSpace == Rgba_1010102 && osVersion < 33) {
        std::string errorString =
                "Color Config RGBA_1010102 supported only 33+ OS version but current is: " +
                std::to_string(osVersion);
        throwException(env, errorString);
        return static_cast<jbyteArray>(nullptr);
    }

    if (preferredColorSpace == Rgba_F16 && osVersion < 26) {
        std::string errorString =
                "Color Config RGBA_1010102 supported only 26+ OS version but current is: " +
                std::to_string(osVersion);
        throwException(env, errorString);
        return static_cast<jbyteArray>(nullptr);
    }

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

    const char *colorSpaceName = nullptr;

    auto nclxColorProfile = heif_image_handle_get_nclx_color_profile(handle, &colorProfileNclx);
    if (nclxColorProfile.code == heif_error_Ok) {
        if (colorProfileNclx && colorProfileNclx->color_primaries != 0 &&
            colorProfileNclx->transfer_characteristics != 0) {
            auto transfer = colorProfileNclx->transfer_characteristics;
            auto colorPrimaries = colorProfileNclx->color_primaries;
            if (colorPrimaries == heif_color_primaries_ITU_R_BT_2020_2_and_2100_0 &&
                transfer == heif_transfer_characteristic_ITU_R_BT_2100_0_PQ) {
                colorSpaceName = "BT2020_PQ";
            } else if (colorPrimaries == heif_color_primaries_ITU_R_BT_709_5 &&
                       transfer == heif_transfer_characteristic_linear) {
                colorSpaceName = "LINEAR_SRGB";
            } else if (colorPrimaries == heif_color_primaries_ITU_R_BT_2020_2_and_2100_0 &&
                       transfer == heif_transfer_characteristic_ITU_R_BT_2100_0_HLG) {
                colorSpaceName = "BT2020_HLG";
            } else if (colorPrimaries == heif_color_primaries_ITU_R_BT_709_5 &&
                       transfer == heif_transfer_characteristic_ITU_R_BT_709_5) {
                colorSpaceName = "BT709";
            } else if (colorPrimaries == heif_color_primaries_ITU_R_BT_2020_2_and_2100_0 &&
                       transfer == heif_transfer_characteristic_linear) {
                hasICC = true;
                profile.resize(sizeof(linearExtendedBT2020));
                std::copy(&linearExtendedBT2020[0],
                          &linearExtendedBT2020[0] + sizeof(linearExtendedBT2020), profile.begin());
            } else if (colorPrimaries == heif_color_primaries_ITU_R_BT_2020_2_and_2100_0 &&
                       (transfer == heif_transfer_characteristic_ITU_R_BT_2020_2_10bit ||
                        transfer == heif_transfer_characteristic_ITU_R_BT_2020_2_12bit)) {
                colorSpaceName = "BT2020";
            } else if (colorPrimaries == heif_color_primaries_SMPTE_EG_432_1 &&
                       transfer == heif_transfer_characteristic_ITU_R_BT_2100_0_HLG) {
                colorSpaceName = "DISPLAY_P3_HLG";
            } else if (colorPrimaries == heif_color_primaries_SMPTE_EG_432_1 &&
                       transfer == heif_transfer_characteristic_ITU_R_BT_2100_0_PQ) {
                colorSpaceName = "DISPLAY_P3_PQ";
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

    int imageWidth;
    int imageHeight;
    int stride;
    std::vector<uint8_t> initialData;

    if (scaledHeight > 0 && scaledWidth > 0) {
        if (bitDepth == 8) {
            auto data = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);
            if (!data) {
                heif_image_release(img);
                heif_image_handle_release(handle);
                throwCannotReadFileException(env);
                return nullptr;
            }
            imageWidth = heif_image_get_width(img, heif_channel_interleaved);
            imageHeight = heif_image_get_height(img, heif_channel_interleaved);
            if (result.code != heif_error_Ok) {
                heif_image_release(img);
                heif_image_handle_release(handle);
                throwInvalidScale(env, result.message);
                return static_cast<jobject>(nullptr);
            }

            // For unknown reason some images doesn't scale properly by libheif in 8-bit
            initialData.resize(scaledWidth * 4 * scaledHeight);
            libyuv::ARGBScale(data, stride, imageWidth, imageHeight, initialData.data(),
                              scaledWidth * 4, scaledWidth, scaledHeight, libyuv::kFilterBox);
            stride = scaledWidth * 4;
            imageHeight = scaledHeight;
            imageWidth = scaledWidth;
            heif_image_release(img);
        } else {
            heif_image *scaledImg;
            result = heif_image_scale_image(img, &scaledImg, scaledWidth, scaledHeight, nullptr);
            if (result.code != heif_error_Ok) {
                heif_image_release(img);
                heif_image_handle_release(handle);
                throwInvalidScale(env, result.message);
                return static_cast<jobject>(nullptr);
            }

            heif_image_release(img);

            auto data = heif_image_get_plane_readonly(scaledImg, heif_channel_interleaved, &stride);
            if (!data) {
                heif_image_release(scaledImg);
                heif_image_handle_release(handle);
                throwCannotReadFileException(env);
                return nullptr;
            }
            imageWidth = heif_image_get_width(scaledImg, heif_channel_interleaved);
            imageHeight = heif_image_get_height(scaledImg, heif_channel_interleaved);
            initialData.resize(stride * imageHeight);
            coder::CopyUnalignedRGBA(reinterpret_cast<const uint8_t *>(data), stride,
                                     reinterpret_cast<uint8_t *>(initialData.data()), stride,
                                     imageWidth,
                                     imageHeight, useBitmapHalf16Floats ? 2 : 1);
            heif_image_release(scaledImg);
        }

    } else {
        auto data = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);
        if (!data) {
            heif_image_release(img);
            heif_image_handle_release(handle);
            throwCannotReadFileException(env);
            return nullptr;
        }

        imageWidth = heif_image_get_width(img, heif_channel_interleaved);
        imageHeight = heif_image_get_height(img, heif_channel_interleaved);
        initialData.resize(stride * imageHeight);

        coder::CopyUnalignedRGBA(reinterpret_cast<const uint8_t *>(data), stride,
                                 reinterpret_cast<uint8_t *>(initialData.data()), stride,
                                 imageWidth,
                                 imageHeight, useBitmapHalf16Floats ? 2 : 1);

        heif_image_release(img);
    }

    std::shared_ptr<uint8_t> dstARGB(static_cast<uint8_t *>(malloc(stride * imageHeight)),
                                     [](uint8_t *f) { free(f); });

    if (useBitmapHalf16Floats) {
        const float scale = 1.0f / float((1 << bitDepth) - 1);
        libyuv::HalfFloatPlane(reinterpret_cast<const uint16_t *>(initialData.data()),
                               stride,
                               reinterpret_cast<uint16_t *>(dstARGB.get()),
                               stride,
                               scale,
                               imageWidth * 4,
                               imageHeight);

    } else {
        std::copy(initialData.begin(), initialData.end(), dstARGB.get());
    }

    initialData.clear();

    heif_image_handle_release(handle);

    if (hasICC) {
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, profile.data(),
                      profile.size(),
                      useBitmapHalf16Floats, &stride);
    } else if (colorSpaceName && strcmp(colorSpaceName, "BT2020_PQ") == 0) {
        if (useBitmapHalf16Floats) {
            PerceptualQuantinizer perceptualQuantinizer(reinterpret_cast<uint8_t *>(dstARGB.get()),
                                                        stride, imageWidth, imageHeight, true, true,
                                                        16, Rec2020);
            perceptualQuantinizer.transfer();
        } else {
            PerceptualQuantinizer perceptualQuantinizer(reinterpret_cast<uint8_t *>(dstARGB.get()),
                                                        stride, imageWidth, imageHeight, false,
                                                        false,
                                                        8, Rec2020);
            perceptualQuantinizer.transfer();
        }
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, &bt2020[0],
                      sizeof(bt2020),
                      useBitmapHalf16Floats, &stride);
    } else if (colorSpaceName && strcmp(colorSpaceName, "BT2020_HLG") == 0) {
        coder::ProcessHLG(reinterpret_cast<uint8_t *>(dstARGB.get()),
                          useBitmapHalf16Floats, stride, imageWidth, imageHeight,
                          useBitmapHalf16Floats ? 16 : 8, coder::Rec2020);
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, &bt2020[0],
                      sizeof(bt2020),
                      useBitmapHalf16Floats, &stride);
    } else if (colorSpaceName && strcmp(colorSpaceName, "DISPLAY_P3_HLG") == 0) {
        coder::ProcessHLG(reinterpret_cast<uint8_t *>(dstARGB.get()),
                          useBitmapHalf16Floats, stride, imageWidth, imageHeight,
                          useBitmapHalf16Floats ? 16 : 8, coder::DCIP3);
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, &displayP3[0],
                      sizeof(displayP3),
                      useBitmapHalf16Floats, &stride);
    } else if (colorSpaceName && strcmp(colorSpaceName, "DISPLAY_P3_PQ") == 0) {
        if (useBitmapHalf16Floats) {
            PerceptualQuantinizer perceptualQuantinizer(reinterpret_cast<uint8_t *>(dstARGB.get()),
                                                        stride, imageWidth, imageHeight, true, true,
                                                        16, DCIP3);
            perceptualQuantinizer.transfer();
        } else {
            PerceptualQuantinizer perceptualQuantinizer(reinterpret_cast<uint8_t *>(dstARGB.get()),
                                                        stride, imageWidth, imageHeight, false,
                                                        false,
                                                        8, DCIP3);
            perceptualQuantinizer.transfer();
        }
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, &displayP3[0],
                      sizeof(displayP3),
                      useBitmapHalf16Floats, &stride);
    } else if (colorSpaceName && strcmp(colorSpaceName, "BT2020") == 0) {
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, &bt2020[0],
                      sizeof(bt2020),
                      useBitmapHalf16Floats, &stride);
    } else if (colorSpaceName && strcmp(colorSpaceName, "DISPLAY_P3") == 0) {
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, &displayP3[0],
                      sizeof(displayP3),
                      useBitmapHalf16Floats, &stride);
    } else if (colorSpaceName && strcmp(colorSpaceName, "LINEAR_SRGB") == 0) {
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, &linearSRGB[0],
                      sizeof(linearSRGB), useBitmapHalf16Floats, &stride);
    } else if (colorSpaceName && strcmp(colorSpaceName, "BT709") == 0) {
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, &bt709[0],
                      sizeof(bt709), useBitmapHalf16Floats, &stride);
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

    std::string imageConfig = useBitmapHalf16Floats ? "RGBA_F16" : "ARGB_8888";
    switch (preferredColorSpace) {
        case Rgba_8888:
            if (useBitmapHalf16Floats) {
                int dstStride = imageWidth * 4 * (int) sizeof(uint8_t);
                std::shared_ptr<uint8_t> rgba8888Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::RGBAF16BitToNBitU8(reinterpret_cast<const uint16_t *>(dstARGB.get()),
                                          stride, rgba8888Data.get(), dstStride, imageWidth,
                                          imageHeight, 8);
                stride = dstStride;
                useBitmapHalf16Floats = false;
                imageConfig = "ARGB_8888";
                dstARGB = rgba8888Data;
            }
            break;
        case Rgba_F16:
            if (useBitmapHalf16Floats) {
                break;
            } else {
                int dstStride = imageWidth * 4 * (int) sizeof(uint16_t);
                std::shared_ptr<uint8_t> rgbaF16Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::Rgba8ToF16(dstARGB.get(), stride,
                                  reinterpret_cast<uint16_t *>(rgbaF16Data.get()), dstStride,
                                  imageWidth, imageHeight, bitDepth);
                stride = dstStride;
                useBitmapHalf16Floats = true;
                imageConfig = "RGBA_F16";
                dstARGB = rgbaF16Data;
            }
            break;
        case Rgb_565:
            if (useBitmapHalf16Floats) {
                int dstStride = imageWidth * (int) sizeof(uint16_t);
                std::shared_ptr<uint8_t> rgb565Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::RGBAF16To565(reinterpret_cast<const uint16_t *>(dstARGB.get()), stride,
                                    reinterpret_cast<uint16_t *>(rgb565Data.get()), dstStride,
                                    imageWidth, imageHeight);
                stride = dstStride;
                useBitmapHalf16Floats = false;
                imageConfig = "RGB_565";
                dstARGB = rgb565Data;
                break;
            } else {
                int dstStride = imageWidth * (int) sizeof(uint16_t);
                std::shared_ptr<uint8_t> rgb565Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::Rgba8To565(dstARGB.get(), stride,
                                  reinterpret_cast<uint16_t *>(rgb565Data.get()), dstStride,
                                  imageWidth, imageHeight, bitDepth);
                stride = dstStride;
                useBitmapHalf16Floats = false;
                imageConfig = "RGB_565";
                dstARGB = rgb565Data;
            }
            break;
        case Rgba_1010102:
            if (useBitmapHalf16Floats) {
                int dstStride = imageWidth * 4 * (int) sizeof(uint8_t);
                std::shared_ptr<uint8_t> rgba1010102Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::F16ToRGBA1010102(reinterpret_cast<const uint16_t *>(dstARGB.get()), stride,
                                        reinterpret_cast<uint8_t *>(rgba1010102Data.get()),
                                        dstStride,
                                        imageWidth, imageHeight);
                stride = dstStride;
                useBitmapHalf16Floats = false;
                imageConfig = "RGBA_1010102";
                dstARGB = rgba1010102Data;
                break;
            } else {
                int dstStride = imageWidth * 4 * (int) sizeof(uint8_t);
                std::shared_ptr<uint8_t> rgba1010102Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::Rgba8ToRGBA1010102(reinterpret_cast<const uint8_t *>(dstARGB.get()), stride,
                                          reinterpret_cast<uint8_t *>(rgba1010102Data.get()),
                                          dstStride,
                                          imageWidth, imageHeight);
                stride = dstStride;
                useBitmapHalf16Floats = false;
                imageConfig = "RGBA_1010102";
                dstARGB = rgba1010102Data;
                break;
            }
            break;
        default:
            break;
    }

    jclass bitmapConfig = env->FindClass("android/graphics/Bitmap$Config");
    jfieldID rgba8888FieldID = env->GetStaticFieldID(bitmapConfig, imageConfig.c_str(),
                                                     "Landroid/graphics/Bitmap$Config;");
    jobject rgba8888Obj = env->GetStaticObjectField(bitmapConfig, rgba8888FieldID);

    jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
    jmethodID createBitmapMethodID = env->GetStaticMethodID(bitmapClass, "createBitmap",
                                                            "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jobject bitmapObj = env->CallStaticObjectMethod(bitmapClass, createBitmapMethodID,
                                                    imageWidth, imageHeight, rgba8888Obj);

    AndroidBitmapInfo info;
    if (AndroidBitmap_getInfo(env, bitmapObj, &info) < 0) {
        throwPixelsException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    void *addr;
    if (AndroidBitmap_lockPixels(env, bitmapObj, &addr) != 0) {
        throwPixelsException(env);
        return static_cast<jobject>(nullptr);
    }

    coder::CopyUnalignedRGBA(reinterpret_cast<const uint8_t *>(dstARGB.get()), stride,
                             reinterpret_cast<uint8_t *>(addr), (int) info.stride, (int) info.width,
                             (int) info.height, useBitmapHalf16Floats ? 2 : 1);

    if (AndroidBitmap_unlockPixels(env, bitmapObj) != 0) {
        throwPixelsException(env);
        return static_cast<jobject>(nullptr);
    }

    return bitmapObj;
}