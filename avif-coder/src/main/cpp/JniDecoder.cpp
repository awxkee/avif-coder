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
#include "SizeScaler.h"
#include <android/log.h>
#include <android/data_space.h>
#include <sys/system_properties.h>
#include "icc/lcms2.h"
#include "colorspace/colorspace.h"
#include <algorithm>
#include <limits>
#include "imagebits/HalfFloats.h"
#include "imagebits/RgbaF16bitToNBitU16.h"
#include "imagebits/RgbaF16bitNBitU8.h"
#include "imagebits/Rgb1010102.h"
#include "colorspace/PerceptualQuantinizer.h"
#include "imagebits/CopyUnalignedRGBA.h"
#include "colorspace/HLG.h"
#include "Support.h"
#include "JniBitmap.h"
#include "ReformatBitmap.h"
#include "IccRecognizer.h"
#include "thread"
#include "VulkanRunner.h"
#include <android/asset_manager_jni.h>

jobject decodeImplementationNative(JNIEnv *env, jobject thiz,
                                   AAssetManager *assetManager,
                                   std::vector<uint8_t> &srcBuffer, jint scaledWidth,
                                   jint scaledHeight, jint javaColorSpace, jint javaScaleMode) {
    PreferredColorConfig preferredColorConfig;
    ScaleMode scaleMode;
    if (!checkDecodePreconditions(env, javaColorSpace, &preferredColorConfig, javaScaleMode,
                                  &scaleMode)) {
        return static_cast<jobject>(nullptr);
    }

    std::shared_ptr<heif_context> ctx(heif_context_alloc(),
                                      [](heif_context *c) { heif_context_free(c); });
    if (!ctx) {
        std::string exception = "Can't create HEIF/AVIF decoder due to unknown reason";
        throwException(env, exception);
        return static_cast<jobject>(nullptr);
    }

    heif_context_set_max_decoding_threads(ctx.get(), (int) std::thread::hardware_concurrency());

    auto result = heif_context_read_from_memory_without_copy(ctx.get(), srcBuffer.data(),
                                                             srcBuffer.size(),
                                                             nullptr);
    if (result.code != heif_error_Ok) {
        std::string exception = "Can't read heif file exception";
        throwException(env, exception);
        return static_cast<jobject>(nullptr);
    }

    heif_image_handle *handle;
    result = heif_context_get_primary_image_handle(ctx.get(), &handle);
    if (result.code != heif_error_Ok) {
        std::string exception = "Acquiring an image from file has failed";
        throwException(env, exception);
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
        std::string exception = "Decoding an image has failed";
        throwException(env, exception);
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
    std::string colorProfile;

    RecognizeICC(handle, img, profile, colorProfile);

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

    imageWidth = heif_image_get_width(img, heif_channel_interleaved);
    imageHeight = heif_image_get_height(img, heif_channel_interleaved);
    auto scaleResult = RescaleImage(initialData, env, handle, img, &stride, useBitmapHalf16Floats,
                                    &imageWidth, &imageHeight, scaledWidth, scaledHeight,
                                    scaleMode);
    if (!scaleResult) {
        return static_cast<jobject >(nullptr);
    }

    heif_image_handle_release(handle);

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

    if (!profile.empty()) {
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, profile.data(),
                      profile.size(),
                      useBitmapHalf16Floats, &stride);
    } else if (colorProfile == "BT2020_PQ") {
        bool isVulkanLoaded = loadVulkanRunner();
        bool vulkanWorkerDone = false;
        if (assetManager != nullptr && isVulkanLoaded) {
            std::string kernel = "SMPTE2084_BT2020.comp.spv";
            vulkanWorkerDone = VulkanRunner(kernel, assetManager,
                                            reinterpret_cast<uint8_t *>(dstARGB.get()), imageWidth,
                                            imageHeight, stride,
                                            useBitmapHalf16Floats ? RgbaF16 : RgbaU8);
        }
        if (!vulkanWorkerDone) {
            PerceptualQuantinizer perceptualQuantinizer(
                    reinterpret_cast<uint8_t *>(dstARGB.get()),
                    stride, imageWidth, imageHeight, useBitmapHalf16Floats, useBitmapHalf16Floats,
                    useBitmapHalf16Floats ? 16 : 8, Rec2020);
            perceptualQuantinizer.transfer();
        }
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, &bt2020[0],
                      sizeof(bt2020),
                      useBitmapHalf16Floats, &stride);
    } else if (colorProfile == "BT2020_HLG") {
        bool isVulkanLoaded = loadVulkanRunner();
        bool vulkanWorkerDone = false;
        if (assetManager != nullptr && isVulkanLoaded) {
            std::string kernel = "HLG_BT2020.comp.spv";
            vulkanWorkerDone = VulkanRunner(kernel, assetManager,
                                            reinterpret_cast<uint8_t *>(dstARGB.get()), imageWidth,
                                            imageHeight, stride,
                                            useBitmapHalf16Floats ? RgbaF16 : RgbaU8);
        }
        if (!vulkanWorkerDone) {
            coder::ProcessHLG(reinterpret_cast<uint8_t *>(dstARGB.get()),
                              useBitmapHalf16Floats, stride, imageWidth, imageHeight,
                              useBitmapHalf16Floats ? 16 : 8, coder::Rec2020);
        }
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, &bt2020[0],
                      sizeof(bt2020),
                      useBitmapHalf16Floats, &stride);
    } else if (colorProfile == "DISPLAY_P3_HLG") {
        bool isVulkanLoaded = loadVulkanRunner();
        bool vulkanWorkerDone = false;
        if (assetManager != nullptr && isVulkanLoaded) {
            std::string kernel = "HLG_P3.comp.spv";
            vulkanWorkerDone = VulkanRunner(kernel, assetManager,
                                            reinterpret_cast<uint8_t *>(dstARGB.get()), imageWidth,
                                            imageHeight, stride,
                                            useBitmapHalf16Floats ? RgbaF16 : RgbaU8);
        }
        if (!vulkanWorkerDone) {
            coder::ProcessHLG(reinterpret_cast<uint8_t *>(initialData.data()),
                              useBitmapHalf16Floats, stride, imageWidth, imageHeight,
                              useBitmapHalf16Floats ? 16 : 8, coder::DCIP3);
        }
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, &displayP3[0],
                      sizeof(displayP3),
                      useBitmapHalf16Floats, &stride);
    } else if (colorProfile == "DISPLAY_P3_PQ") {
        bool isVulkanLoaded = loadVulkanRunner();
        bool vulkanWorkerDone = false;
        if (assetManager != nullptr && isVulkanLoaded) {
            std::string kernel = "SMPTE2084_P3.comp.spv";
            vulkanWorkerDone = VulkanRunner(kernel, assetManager,
                                            reinterpret_cast<uint8_t *>(dstARGB.get()), imageWidth,
                                            imageHeight, stride,
                                            useBitmapHalf16Floats ? RgbaF16 : RgbaU8);
        }
        if (!vulkanWorkerDone) {
            PerceptualQuantinizer perceptualQuantinizer(
                    reinterpret_cast<uint8_t *>(dstARGB.get()),
                    stride, imageWidth, imageHeight,
                    useBitmapHalf16Floats,
                    useBitmapHalf16Floats,
                    useBitmapHalf16Floats ? 16 : 8, DCIP3);
            perceptualQuantinizer.transfer();
        }

        convertUseICC(dstARGB, stride, imageWidth, imageHeight, &displayP3[0],
                      sizeof(displayP3),
                      useBitmapHalf16Floats, &stride);
    } else if (colorProfile == "BT2020") {
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, &bt2020[0],
                      sizeof(bt2020),
                      useBitmapHalf16Floats, &stride);
    } else if (colorProfile == "DISPLAY_P3") {
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, &displayP3[0],
                      sizeof(displayP3),
                      useBitmapHalf16Floats, &stride);
    } else if (colorProfile == "LINEAR_SRGB") {
        convertUseICC(dstARGB, stride, imageWidth, imageHeight, &linearSRGB[0],
                      sizeof(linearSRGB), useBitmapHalf16Floats, &stride);
    } else if (colorProfile == "BT709") {
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

    jobject hwBuffer = nullptr;

    ReformatColorConfig(env, dstARGB, imageConfig, preferredColorConfig, bitDepth, imageWidth,
                        imageHeight, &stride, &useBitmapHalf16Floats, &hwBuffer);

    return createBitmap(env, dstARGB, imageConfig, stride, imageWidth, imageHeight,
                        useBitmapHalf16Floats, hwBuffer);
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_decodeImpl(JNIEnv *env, jobject thiz,
                                                            jobject assetManager,
                                                            jbyteArray byte_array, jint scaledWidth,
                                                            jint scaledHeight,
                                                            jint javaColorspace, jint scaleMode) {
    auto totalLength = env->GetArrayLength(byte_array);
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(byte_array, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));
    AAssetManager *manager = nullptr;
    if (assetManager) {
        manager = AAssetManager_fromJava(env, assetManager);
    }
    return decodeImplementationNative(env, thiz, manager, srcBuffer,
                                      scaledWidth, scaledHeight,
                                      javaColorspace, scaleMode);
}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_decodeByteBufferImpl(JNIEnv *env, jobject thiz,
                                                                      jobject byteBuffer,
                                                                      jobject assetManager,
                                                                      jint scaledWidth,
                                                                      jint scaledHeight,
                                                                      jint clrConfig,
                                                                      jint scaleMode) {
    auto bufferAddress = reinterpret_cast<uint8_t *>(env->GetDirectBufferAddress(byteBuffer));
    int length = (int) env->GetDirectBufferCapacity(byteBuffer);
    if (!bufferAddress || length <= 0) {
        std::string errorString = "Only direct byte buffers are supported";
        throwException(env, errorString);
        return nullptr;
    }
    std::vector<uint8_t> srcBuffer(length);
    std::copy(bufferAddress, bufferAddress + length, srcBuffer.begin());
    AAssetManager *manager = nullptr;
    if (assetManager) {
        manager = AAssetManager_fromJava(env, assetManager);
    }
    return decodeImplementationNative(env, thiz, manager, srcBuffer,
                                      scaledWidth, scaledHeight,
                                      clrConfig, scaleMode);
}