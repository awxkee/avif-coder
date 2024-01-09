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
#include <cmath>
#include <limits>
#include "imagebits/RgbaF16bitToNBitU16.h"
#include "imagebits/RgbaF16bitNBitU8.h"
#include "imagebits/Rgb1010102.h"
#include "colorspace/HDRTransferAdapter.h"
#include "imagebits/CopyUnalignedRGBA.h"
#include "imagebits/RGBAlpha.h"
#include "imagebits/Rgb565.h"

using namespace std;

struct AvifMemEncoder {
    std::vector<char> buffer;
};

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
        std::string exception = "Can't create HEIF/AVIF encoder due to unknown reason";
        throwException(env, exception);
        return static_cast<jbyteArray>(nullptr);
    }

    heif_encoder *mEncoder;
    auto result = heif_context_get_encoder_for_format(ctx.get(), heifCompressionFormat, &mEncoder);
    if (result.code != heif_error_Ok) {
        std::string choke(result.message);
        std::string str = "Can't create encoder with exception: " + choke;
        throwException(env, str);
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
        std::string choke(result.message);
        std::string str = "Can't create encoded image with exception: " + choke;
        throwException(env, str);
        return static_cast<jbyteArray>(nullptr);
    }

    int bitDepth = 8;
    if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        bitDepth = 8;
    } else if ((info.format == ANDROID_BITMAP_FORMAT_RGBA_F16 &&
                heifCompressionFormat == heif_compression_AV1) ||
               (info.format == ANDROID_BITMAP_FORMAT_RGBA_1010102 &&
                heifCompressionFormat == heif_compression_AV1)) {
        bitDepth = 10;
    }

    result = heif_image_add_plane(image, heif_channel_interleaved, (int) info.width,
                                  (int) info.height, bitDepth);
    if (result.code != heif_error_Ok) {
        std::string choke(result.message);
        std::string str = "Can't create add plane to encoded image with exception: " + choke;
        throwException(env, str);
        return static_cast<jbyteArray>(nullptr);
    }
    int stride;
    uint8_t *imgData = heif_image_get_plane(image, heif_channel_interleaved, &stride);
    if (info.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
        coder::UnpremultiplyRGBA(sourceData.data(), (int) info.stride,
                                 imgData,
                                 stride, (int) info.width, (int) info.height);
        heif_image_set_premultiplied_alpha(image, false);
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        coder::Rgb565ToUnsigned8(reinterpret_cast<uint16_t *>(sourceData.data()), (int) info.stride,
                                 imgData, stride, (int) info.width, (int) info.height, 8, 255);
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
            RGBAF16BitToNBitU16(reinterpret_cast<const uint16_t *>(sourceData.data()),
                                (int) info.stride,
                                reinterpret_cast<uint16_t *>(imgData), stride,
                                (int) info.width,
                                (int) info.height, 10);
        } else {
            coder::RGBAF16BitToNBitU8(reinterpret_cast<const uint16_t *>(sourceData.data()),
                                      (int) info.stride,
                                      reinterpret_cast<uint8_t *>(imgData), stride,
                                      (int) info.width,
                                      (int) info.height, 8);
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

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_encodeAvifImpl(JNIEnv *env, jobject thiz,
                                                                jobject bitmap, jint quality) {
    try {
        return encodeBitmap(env, thiz, bitmap, heif_compression_AV1, quality);
    } catch (std::bad_alloc &err) {
        std::string exception = "Not enough memory to encode this image";
        throwException(env, exception);
        return static_cast<jbyteArray>(nullptr);
    }
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_encodeHeicImpl(JNIEnv *env, jobject thiz,
                                                                jobject bitmap, jint quality) {
    try {
        return encodeBitmap(env, thiz, bitmap, heif_compression_HEVC, quality);
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
                                                             jbyteArray byte_array) {
    try {
        std::shared_ptr<heif_context> ctx(heif_context_alloc(),
                                          [](heif_context *c) { heif_context_free(c); });
        if (!ctx) {
            return static_cast<jobject>(nullptr);
        }
        auto totalLength = env->GetArrayLength(byte_array);
        std::vector<uint8_t> srcBuffer(totalLength);
        env->GetByteArrayRegion(byte_array, 0, totalLength,
                                reinterpret_cast<jbyte *>(srcBuffer.data()));
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