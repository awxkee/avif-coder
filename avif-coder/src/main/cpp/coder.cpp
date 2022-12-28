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
    heif_context *ctx = heif_context_alloc();

    heif_encoder *encoder;
    auto result = heif_context_get_encoder_for_format(ctx, heifCompressionFormat, &encoder);
    if (result.code != heif_error_Ok) {
        heif_context_free(ctx);
        throwCantEncodeImageException(env);
        return static_cast<jbyteArray>(nullptr);
    }
    heif_encoder_set_lossy_quality(encoder, quality);

    AndroidBitmapInfo info;
    int ret;
    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
        throwCantEncodeImageException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        throwCantEncodeImageException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    void *addr;
    if ((ret = AndroidBitmap_lockPixels(env, bitmap, &addr)) != 0) {
        throwCantEncodeImageException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    heif_image *image;
    result = heif_image_create((int) info.width, (int) info.height, heif_colorspace_RGB,
                               heif_chroma_interleaved_RGBA, &image);
    if (result.code != heif_error_Ok) {
        AndroidBitmap_unlockPixels(env, bitmap);
        heif_encoder_release(encoder);
        heif_context_free(ctx);
        throwCantEncodeImageException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    result = heif_image_add_plane(image, heif_channel_interleaved, (int) info.width,
                                  (int) info.height, 32);
    if (result.code != heif_error_Ok) {
        AndroidBitmap_unlockPixels(env, bitmap);
        heif_encoder_release(encoder);
        heif_context_free(ctx);
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
    auto options = heif_encoding_options_alloc();
    options->version = 5;
    options->image_orientation = heif_orientation_normal;
    result = heif_context_encode_image(ctx, image, encoder, options, &handle);
    heif_encoding_options_free(options);
    if (handle) {
        heif_context_set_primary_image(ctx, handle);
        heif_image_handle_release(handle);
    }
    heif_image_release(image);
    if (result.code != heif_error_Ok) {
        heif_encoder_release(encoder);
        heif_context_free(ctx);
        throwCantEncodeImageException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    heif_encoder_release(encoder);

    std::vector<char> buf;
    heif_writer writer = {};
    writer.writer_api_version = 1;
    writer.write = writeHeifData;
    AvifMemEncoder memEncoder;
    result = heif_context_write(ctx, &writer, &memEncoder);
    if (result.code != heif_error_Ok) {
        heif_context_free(ctx);
        throwCantEncodeImageException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    heif_context_free(ctx);
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
JNIEXPORT jobject JNICALL
Java_com_radzivon_bartoshyk_avif_coder_HeifCoder_decodeAvifImpl(JNIEnv *env, jobject thiz,
                                                                jbyteArray byte_array) {
    heif_context *ctx = heif_context_alloc();
    auto totalLength = env->GetArrayLength(byte_array);
    auto srcBuffer = malloc(totalLength);
    env->GetByteArrayRegion(byte_array, 0, totalLength, reinterpret_cast<jbyte *>(srcBuffer));
    auto result = heif_context_read_from_memory_without_copy(ctx, srcBuffer, totalLength, nullptr);
    if (result.code != heif_error_Ok) {
        free(srcBuffer);
        heif_context_free(ctx);
        throwCannotReadFileException(env);
        return static_cast<jobject>(nullptr);
    }

    heif_image_handle *handle;
    result = heif_context_get_primary_image_handle(ctx, &handle);
    if (result.code != heif_error_Ok) {
        free(srcBuffer);
        heif_context_free(ctx);
        throwCannotReadFileException(env);
        return static_cast<jobject>(nullptr);
    }
    heif_image *img;
    auto options = heif_decoding_options_alloc();
    options->convert_hdr_to_8bit = true;
    result = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_interleaved_RGBA,
                               nullptr);
    heif_decoding_options_free(options);
    if (result.code != heif_error_Ok) {
        heif_image_handle_release(handle);
        free(srcBuffer);
        heif_context_free(ctx);
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

    free(srcBuffer);
    heif_image_release(img);
    heif_image_handle_release(handle);
    heif_context_free(ctx);

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