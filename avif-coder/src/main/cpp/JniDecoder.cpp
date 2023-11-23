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
#include "imagebits/RgbaF16bitToNBitU16.h"
#include "imagebits/RgbaF16bitNBitU8.h"
#include "imagebits/Rgb1010102.h"
#include "colorspace/HDRTransferAdapter.h"
#include "imagebits/CopyUnalignedRGBA.h"
#include "Support.h"
#include "JniBitmap.h"
#include "ReformatBitmap.h"
#include "IccRecognizer.h"
#include "thread"
#include "VulkanRunner.h"
#include <android/asset_manager_jni.h>
#include "imagebits/RGBAlpha.h"
#include "imagebits/RgbaU16toHF.h"

using namespace std;

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

    heif_image_handle *handlePtr;
    result = heif_context_get_primary_image_handle(ctx.get(), &handlePtr);
    if (result.code != heif_error_Ok || handlePtr == nullptr) {
        std::string exception = "Acquiring an image from file has failed";
        throwException(env, exception);
        return static_cast<jobject>(nullptr);
    }

    std::shared_ptr<heif_image_handle> handle(handlePtr, [](heif_image_handle *hd) {
        heif_image_handle_release(hd);
    });

    int bitDepth = heif_image_handle_get_chroma_bits_per_pixel(handle.get());
    bool useBitmapHalf16Floats = false;
    if (bitDepth < 0) {
        throwBitDepthException(env);
        return static_cast<jobject>(nullptr);
    }

    int osVersion = androidOSVersion();

    if (bitDepth > 8 && osVersion >= 26) {
        useBitmapHalf16Floats = true;
    }
    heif_image *imgPtr;
    std::shared_ptr<heif_decoding_options> options(heif_decoding_options_alloc(),
                                                   [](heif_decoding_options *deo) {
                                                       heif_decoding_options_free(deo);
                                                   });
    options->convert_hdr_to_8bit = false;
    options->ignore_transformations = false;
    result = heif_decode_image(handle.get(), &imgPtr, heif_colorspace_RGB,
                               useBitmapHalf16Floats ? heif_chroma_interleaved_RRGGBBAA_LE
                                                     : heif_chroma_interleaved_RGBA,
                               options.get());
    options.reset();

    if (result.code != heif_error_Ok || imgPtr == nullptr) {
        std::string exception = "Decoding an image has failed";
        throwException(env, exception);
        return static_cast<jobject>(nullptr);
    }

    std::shared_ptr<heif_image> img(imgPtr, [](heif_image *im) {
        heif_image_release(im);
    });

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
    if (heif_image_handle_has_alpha_channel(handle.get())) {
        alphaPremultiplied = heif_image_handle_is_premultiplied_alpha(handle.get()) != 0;
    } else {
        alphaPremultiplied = false;
    }

    int imageWidth;
    int imageHeight;
    int stride;
    std::vector<uint8_t> initialData;

    imageWidth = heif_image_get_width(img.get(), heif_channel_interleaved);
    imageHeight = heif_image_get_height(img.get(), heif_channel_interleaved);
    auto scaleResult = RescaleImage(initialData, env, handle, img, &stride, useBitmapHalf16Floats,
                                    &imageWidth, &imageHeight, scaledWidth, scaledHeight,
                                    scaleMode);
    if (!scaleResult) {
        return static_cast<jobject >(nullptr);
    }

    img.reset();
    handle.reset();

    if (alphaPremultiplied && preferredColorConfig != Rgba_8888 && !useBitmapHalf16Floats) {
        // Remove premultiplied alpha because only in RGBA 8888 it is required
        coder::UnpremultiplyRGBA(reinterpret_cast<const uint8_t *>(initialData.data()),
                                 stride,
                                 reinterpret_cast<uint8_t *>(initialData.data()),
                                 stride, imageWidth, imageHeight);
        alphaPremultiplied = false;
    }

    vector<uint8_t> dstARGB(stride * imageHeight);

    if (useBitmapHalf16Floats) {
        coder::RgbaU16ToF(reinterpret_cast<const uint16_t *>(initialData.data()),
                          stride,
                          reinterpret_cast<uint16_t *>(dstARGB.data()),
                          stride,
                          imageWidth * 4, imageHeight,
                          bitDepth);

    } else {
        dstARGB = initialData;
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
                                            reinterpret_cast<uint8_t *>(dstARGB.data()), imageWidth,
                                            imageHeight, stride,
                                            useBitmapHalf16Floats ? RgbaF16 : RgbaU8);
        }
        if (!vulkanWorkerDone) {
            HDRTransferAdapter hdrTransferAdapter(
                    reinterpret_cast<uint8_t *>(dstARGB.data()),
                    stride, imageWidth, imageHeight, useBitmapHalf16Floats, useBitmapHalf16Floats,
                    useBitmapHalf16Floats ? 16 : 8, Rec2020, PQ);
            hdrTransferAdapter.transfer();
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
                                            reinterpret_cast<uint8_t *>(dstARGB.data()), imageWidth,
                                            imageHeight, stride,
                                            useBitmapHalf16Floats ? RgbaF16 : RgbaU8);
        }
        if (!vulkanWorkerDone) {
            HDRTransferAdapter hdrTransferAdapter(
                    reinterpret_cast<uint8_t *>(dstARGB.data()),
                    stride, imageWidth, imageHeight,
                    useBitmapHalf16Floats,
                    useBitmapHalf16Floats,
                    useBitmapHalf16Floats ? 16 : 8, DCIP3, HLG);
            hdrTransferAdapter.transfer();
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
                                            reinterpret_cast<uint8_t *>(dstARGB.data()), imageWidth,
                                            imageHeight, stride,
                                            useBitmapHalf16Floats ? RgbaF16 : RgbaU8);
        }
        if (!vulkanWorkerDone) {
            HDRTransferAdapter hdrTransferAdapter(
                    reinterpret_cast<uint8_t *>(dstARGB.data()),
                    stride, imageWidth, imageHeight,
                    useBitmapHalf16Floats,
                    useBitmapHalf16Floats,
                    useBitmapHalf16Floats ? 16 : 8, DCIP3, HLG);
            hdrTransferAdapter.transfer();
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
                                            reinterpret_cast<uint8_t *>(dstARGB.data()), imageWidth,
                                            imageHeight, stride,
                                            useBitmapHalf16Floats ? RgbaF16 : RgbaU8);
        }
        if (!vulkanWorkerDone) {
            HDRTransferAdapter hdrTransferAdapter(
                    reinterpret_cast<uint8_t *>(dstARGB.data()),
                    stride, imageWidth, imageHeight,
                    useBitmapHalf16Floats,
                    useBitmapHalf16Floats,
                    useBitmapHalf16Floats ? 16 : 8, DCIP3, PQ);
            hdrTransferAdapter.transfer();
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
            libyuv::ARGBAttenuate(reinterpret_cast<uint8_t *>(dstARGB.data()), stride,
                                  reinterpret_cast<uint8_t *>(dstARGB.data()), stride, imageWidth,
                                  imageHeight);
        }
    }

    std::string imageConfig = useBitmapHalf16Floats ? "RGBA_F16" : "ARGB_8888";

    jobject hwBuffer = nullptr;

    try {
        coder::ReformatColorConfig(env, ref(dstARGB), ref(imageConfig), preferredColorConfig,
                                   bitDepth, imageWidth,
                                   imageHeight, &stride, &useBitmapHalf16Floats, &hwBuffer,
                                   alphaPremultiplied);

        return createBitmap(env, ref(dstARGB), imageConfig, stride, imageWidth, imageHeight,
                            useBitmapHalf16Floats, hwBuffer);
    } catch (coder::ReformatBitmapError &err) {
        std::string exception(err.what());
        throwException(env, exception);
        return static_cast<jobject>(nullptr);
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_decodeImpl(JNIEnv *env, jobject thiz,
                                                            jobject assetManager,
                                                            jbyteArray byte_array, jint scaledWidth,
                                                            jint scaledHeight,
                                                            jint javaColorspace, jint scaleMode) {
    try {
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
    } catch (std::bad_alloc &err) {
        std::string exception = "Not enough memory to decode this image";
        throwException(env, exception);
        return static_cast<jobject>(nullptr);
    }
}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_decodeByteBufferImpl(JNIEnv *env, jobject thiz,
                                                                      jobject assetManager,
                                                                      jobject byteBuffer,
                                                                      jint scaledWidth,
                                                                      jint scaledHeight,
                                                                      jint clrConfig,
                                                                      jint scaleMode) {
    try {
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
    } catch (std::bad_alloc &err) {
        std::string exception = "Not enough memory to decode this image";
        throwException(env, exception);
        return static_cast<jobject>(nullptr);
    }
}