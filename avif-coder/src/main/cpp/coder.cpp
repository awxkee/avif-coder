#include <jni.h>
#include <string>
#include "libheif/heif.h"
#include "libyuv/libyuv.h"
#include "android/bitmap.h"
#include "libyuv/convert_argb.h"
#include <vector>

extern "C" JNIEXPORT jstring JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

jint throwCannotReadFileException(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/CantReadHeifFileException");
    return env->ThrowNew(exClass, "");
}

jint throwCoderCreationException(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/CantCreateCodecException");
    return env->ThrowNew(exClass, "");
}

jint throwCantDecodeImageException(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/CantDecoderImageException");
    return env->ThrowNew(exClass, "");
}

jint throwCantEncodeImageException(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/CantEncodeImageException");
    return env->ThrowNew(exClass, "");
}

jint throwPixelsException(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/radzivon/bartoshyk/avif/coder/GetPixelsException");
    return env->ThrowNew(exClass, "");
}

struct AvifMemEncoder {
    std::vector<char> buffer;
};

struct heif_error writeHeifData(struct heif_context *ctx, // TODO: why do we need this parameter?
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
        throwCantEncodeImageException(env);
        return static_cast<jbyteArray>(nullptr);
    }
    std::shared_ptr<heif_encoder> encoder(mEncoder,
                                          [](heif_encoder *he) { heif_encoder_release(he); });
    heif_encoder_set_lossy_quality(encoder.get(), quality);
    AndroidBitmapInfo info;
    int ret;
    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
        throwPixelsException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        throwPixelsException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    void *addr;
    if ((ret = AndroidBitmap_lockPixels(env, bitmap, &addr)) != 0) {
        throwPixelsException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    heif_image *image;
    result = heif_image_create((int) info.width, (int) info.height, heif_colorspace_RGB,
                               heif_chroma_interleaved_RGBA, &image);
    if (result.code != heif_error_Ok) {
        AndroidBitmap_unlockPixels(env, bitmap);
        throwCantEncodeImageException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    result = heif_image_add_plane(image, heif_channel_interleaved, (int) info.width,
                                  (int) info.height, 32);
    if (result.code != heif_error_Ok) {
        AndroidBitmap_unlockPixels(env, bitmap);
        throwCantEncodeImageException(env);
        return static_cast<jbyteArray>(nullptr);
    }
    int stride;
    uint8_t *imgData = heif_image_get_plane(image, heif_channel_interleaved, &stride);
    auto srcARGB = malloc(info.height * stride);
    libyuv::RGBAToARGB(reinterpret_cast<const uint8_t *>(addr), (int) info.stride,
                       static_cast<uint8_t *>(srcARGB), stride,
                       (int) info.width, (int) info.height);
    libyuv::ARGBToRGBA(reinterpret_cast<const uint8_t *>(srcARGB), (int) stride, imgData, stride,
                       (int) info.width, (int) info.height);
    free(srcARGB);
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
    if (handle) {
        heif_context_set_primary_image(ctx.get(), handle);
        heif_image_handle_release(handle);
    }
    heif_image_release(image);
    if (result.code != heif_error_Ok) {
        throwCantEncodeImageException(env);
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
        throwCantEncodeImageException(env);
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
                                                                jobject bitmap) {
    return encodeBitmap(env, thiz, bitmap, heif_compression_AV1, 70);
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_encodeHeicImpl(JNIEnv *env, jobject thiz,
                                                                jobject bitmap) {
    return encodeBitmap(env, thiz, bitmap, heif_compression_HEVC, 70);
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
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_decodeImpl(JNIEnv *env, jobject thiz,
                                                            jbyteArray byte_array) {
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
    heif_image *img;
    std::shared_ptr<heif_decoding_options> options(heif_decoding_options_alloc(),
                                                   [](heif_decoding_options *deo) {
                                                       heif_decoding_options_free(deo);
                                                   });
    options->convert_hdr_to_8bit = true;
    result = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_interleaved_RGBA,
                               nullptr);
    options.reset();
    if (result.code != heif_error_Ok) {
        heif_image_handle_release(handle);
        throwCantDecodeImageException(env);
        return static_cast<jobject>(nullptr);
    }

    int stride;
    const uint8_t *data = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);

    auto imageWidth = heif_image_get_width(img, heif_channel_interleaved);
    auto imageHeight = heif_image_get_height(img, heif_channel_interleaved);

    char *dstARGB = static_cast<char *>(malloc(stride * imageHeight));

    int i;
    char tmpR;
    char tmpG;
    char tmpB;
    char tmpA;
    for (i = 0; i < stride * imageHeight; i += 4) {
        //swap R and B; raw_image[i + 1] is G, so it stays where it is.
        tmpR = data[i];
        tmpG = data[i + 1];
        tmpB = data[i + 2];
        tmpA = data[i + 3];
        dstARGB[i + 0] = tmpB;
        dstARGB[i + 1] = tmpG;
        dstARGB[i + 2] = tmpR;
        dstARGB[i + 3] = tmpA;
    }

    heif_image_release(img);
    heif_image_handle_release(handle);

    jclass bitmapConfig = env->FindClass("android/graphics/Bitmap$Config");
    jfieldID rgba8888FieldID = env->GetStaticFieldID(bitmapConfig, "ARGB_8888",
                                                     "Landroid/graphics/Bitmap$Config;");
    jobject rgba8888Obj = env->GetStaticObjectField(bitmapConfig, rgba8888FieldID);

    jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
    jmethodID createBitmapMethodID = env->GetStaticMethodID(bitmapClass, "createBitmap",
                                                            "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jobject bitmapObj = env->CallStaticObjectMethod(bitmapClass, createBitmapMethodID,
                                                    imageWidth, imageHeight, rgba8888Obj);
    auto returningLength = stride * imageHeight;
    jintArray pixels = env->NewIntArray(stride * imageHeight);
    env->SetIntArrayRegion(pixels, 0, (jsize) returningLength / sizeof(uint32_t),
                           reinterpret_cast<const jint *>(dstARGB));
    jmethodID setPixelsMid = env->GetMethodID(bitmapClass, "setPixels", "([IIIIIII)V");
    env->CallVoidMethod(bitmapObj, setPixelsMid, pixels, 0,
                        static_cast<jint >(stride / sizeof(uint32_t)), 0, 0, imageWidth,
                        imageHeight);
    env->DeleteLocalRef(pixels);

    return bitmapObj;
}