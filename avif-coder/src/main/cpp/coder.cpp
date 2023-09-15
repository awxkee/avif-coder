#include <jni.h>
#include <string>
#include "libheif/heif.h"
#include "libyuv/libyuv.h"
#include "android/bitmap.h"
#include "libyuv/convert_argb.h"
#include <vector>
#include "jni_exception.h"
#include "scaler.h"
#include <android/log.h>
#include <android/data_space.h>
#include <sys/system_properties.h>
#include "icc/lcms2.h"
#include "colorspace.h"
#include <math.h>
#include <limits>
#include "attenuate_alpha.h"
#include "HalfFloats.h"
#include "RgbaF16bitToNBitU16.h"
#include "RgbaF16bitNBitU8.h"
#include "Rgb1010102.h"
#include "PerceptualQuantinizer.h"
#include "CopyUnalignedRGBA.h"
#include "HLG.h"

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

    std::vector<uint8_t> sourceData(info.height * info.stride);
    std::copy(reinterpret_cast<uint8_t *>(addr),
              reinterpret_cast<uint8_t *>(addr) + info.height * info.stride, sourceData.data());

    AndroidBitmap_unlockPixels(env, bitmap);

    heif_image *image;
    heif_chroma chroma = heif_chroma_interleaved_RGBA;
    if ((info.format == ANDROID_BITMAP_FORMAT_RGBA_F16 &&
         heifCompressionFormat == heif_compression_AV1) ||
        (info.format == ANDROID_BITMAP_FORMAT_RGBA_1010102 &&
         heifCompressionFormat == heif_compression_AV1)) {
        chroma = heif_chroma_interleaved_RRGGBBAA_LE;
    }
    result = heif_image_create((int) info.width, (int) info.height, heif_colorspace_RGB,
                               chroma, &image);
    if (result.code != heif_error_Ok) {
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
        throwCantEncodeImageException(env, result.message);
        return static_cast<jbyteArray>(nullptr);
    }
    int stride;
    uint8_t *imgData = heif_image_get_plane(image, heif_channel_interleaved, &stride);
    if (info.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
        libyuv::ARGBCopy(sourceData.data(), (int) info.stride,
                         imgData,
                         stride, (int) info.width, (int) info.height);
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        libyuv::RGB565ToARGB(sourceData.data(), (int) info.stride, imgData,
                             stride, (int) info.width, (int) info.height);
        for (int i = 0; i < stride / 4 * info.height; i += 4) {
            imgData[i] = imgData[i + 1]; // R
            imgData[i + 1] = imgData[i + 2]; // G
            imgData[i + 2] = imgData[i + 3]; // B
            imgData[i + 3] = imgData[i]; // A
        }
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGBA_1010102) {
        if (heifCompressionFormat == heif_compression_HEVC) {
            RGBA1010102ToU8(reinterpret_cast<uint8_t *>(sourceData.data()), (int) info.stride,
                            reinterpret_cast<uint8_t *>(imgData), stride, (int) info.width,
                            (int) info.height);
        } else {
            RGBA1010102ToU16(reinterpret_cast<uint8_t *>(sourceData.data()), (int) info.stride,
                             reinterpret_cast<uint16_t *>(imgData), stride, (int) info.width,
                             (int) info.height);
        }
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGBA_F16) {
        if (heifCompressionFormat == heif_compression_AV1) {
            std::shared_ptr<char> dstARGB(
                    static_cast<char *>(malloc(info.width * info.height * 4 * sizeof(uint16_t))),
                    [](char *f) { free(f); });
            int dstStride = (int) info.width * 4 * (int) sizeof(uint16_t);

            RGBAF16BitToNBitU16(reinterpret_cast<const uint16_t *>(sourceData.data()),
                                (int) info.stride,
                                reinterpret_cast<uint16_t *>(dstARGB.get()), dstStride,
                                (int) info.width,
                                (int) info.height, 10);
            coder::CopyUnalignedRGBA(reinterpret_cast<uint8_t *>(dstARGB.get()), dstStride,
                                     reinterpret_cast<uint8_t *>(imgData), stride,
                                     (int) info.width,
                                     (int) info.height, sizeof(uint16_t));

            dstARGB.reset();
        } else {
            std::shared_ptr<char> dstARGB(
                    static_cast<char *>(malloc(info.width * info.height * 4 * sizeof(uint8_t))),
                    [](char *f) { free(f); });
            int dstStride = (int) info.width * 4 * (int) sizeof(uint8_t);

            RGBAF16BitToNBitU8(reinterpret_cast<const uint16_t *>(sourceData.data()),
                               (int) info.stride,
                               reinterpret_cast<uint8_t *>(dstARGB.get()), dstStride,
                               (int) info.width,
                               (int) info.height, 8);

            libyuv::ARGBCopy(reinterpret_cast<uint8_t *>(dstARGB.get()), (int) dstStride,
                             reinterpret_cast<uint8_t *>(imgData), stride, (int) info.width,
                             (int) info.height);
            dstARGB.reset();
        }
    }

    heif_image_handle *handle;
    std::shared_ptr<heif_encoding_options> options(heif_encoding_options_alloc(),
                                                   [](heif_encoding_options *o) {
                                                       heif_encoding_options_free(o);
                                                   });
    options->version = 5;
    options->image_orientation = heif_orientation_normal;
//    options->output_nclx_profile = nullptr;
//    options->save_two_colr_boxes_when_ICC_and_nclx_available = false;

    result = heif_context_encode_image(ctx.get(), image, encoder.get(), options.get(), &handle);
    options.reset();
    if (handle && result.code == heif_error_Ok) {
        heif_context_set_primary_image(ctx.get(), handle);
//
//        std::time_t currentTime = std::time(nullptr);
//        std::tm *timeInfo = std::localtime(&currentTime);
//
//        // Format the date and time
//        char formattedTime[20]; // Buffer for the formatted time
//        std::strftime(formattedTime, sizeof(formattedTime), "%Y:%m:%d %H:%M:%S", timeInfo);
//        std::string dateTime(formattedTime);
//
//        std::string format(heifCompressionFormat == heif_compression_AV1 ? "AVIF" : "HEIC");
//
//        std::string xmpMetadata = "<?xpacket begin='' id='W5M0MpCehiHzreSzNTczkc9d'?>"
//                                  "<x:xmpmeta xmlns:x='adobe:ns:meta/' x:xmptk='XMP Core 5.5.0'>"
//                                  "<rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'>"
//                                  "<rdf:Description rdf:about='' xmlns:dc='http://purl.org/dc/elements/1.1/'>"
//                                  "<dc:title>Generated image by avif-coder</dc:title>"
//                                  "<dc:creator>avif-coder</dc:creator>"
//                                  "<dc:description>A image was created by avif-coder (https://github.com/awxkee/avif-coder)</dc:description>"
//                                  "<dc:date>" + dateTime + "</dc:date>\n"
//                                                           "<dc:publisher>https://github.com/awxkee/avif-coder>"
//                                                           "<dc:format>" + format + "</dc:format>"
//                                                                                    "</rdf:Description>"
//                                                                                    "<rdf:Description rdf:about='' xmlns:exif='http://ns.adobe.com/exif/1.0/'>\n"
//                                                                                    "<exif:ColorSpace>sRGB</exif:ColorSpace>\n"
//                                                                                    "<exif:ColorProfile>sRGB IEC61966-2.1</exif:ColorProfile>\n"
//                                                                                    "</rdf:Description>\n"
//                                                                                    "<rdf:Description rdf:about='' xmlns:xmp='http://ns.adobe.com/xap/1.0/'>\n"
//                                                                                    "<xmp:CreatorTool>avif-coder (https://github.com/awxkee/avif-coder)</xmp:CreatorTool>\n"
//                                                                                    "<xmp:ModifyDate>" +
//                                  dateTime +
//                                  "</xmp:ModifyDate>\n"
//                                  "</rdf:Description>\n"
//                                  "</rdf:RDF>"
//                                  "</x:xmpmeta>"
//                                  "<?xpacket end='w'?>";
//
//        result = heif_context_add_XMP_metadata(ctx.get(), handle,
//                                               reinterpret_cast<const void *>(xmpMetadata.data()),
//                                               static_cast<int>(xmpMetadata.size()));
//        if (result.code != heif_error_Ok) {
//            heif_image_handle_release(handle);
//            heif_image_release(image);
//            throwCantEncodeImageException(env, result.message);
//            return static_cast<jbyteArray>(nullptr);
//        }

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
                          &linearExtendedBT2020[0] + sizeof(linearExtendedBT2020), profile.data());
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