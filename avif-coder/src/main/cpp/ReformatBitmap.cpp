//
// Created by Radzivon Bartoshyk on 16/09/2023.
//

#include "ReformatBitmap.h"
#include "imagebits/RgbaF16bitNBitU8.h"
#include "imagebits/Rgba8ToF16.h"
#include "imagebits/Rgb565.h"
#include "imagebits/Rgb1010102.h"
#include "imagebits/CopyUnalignedRGBA.h"
#include <string>
#include "Support.h"
#include <android/bitmap.h>
#include <HardwareBuffersCompat.h>

void
ReformatColorConfig(JNIEnv *env, std::shared_ptr<uint8_t> &imageData, std::string &imageConfig,
                    PreferredColorConfig preferredColorConfig, int depth,
                    int imageWidth, int imageHeight, int *stride, bool *useFloats, jobject* hwBuffer) {
    *hwBuffer = nullptr;
    switch (preferredColorConfig) {
        case Rgba_8888:
            if (*useFloats) {
                int dstStride = imageWidth * 4 * (int) sizeof(uint8_t);
                std::shared_ptr<uint8_t> rgba8888Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::RGBAF16BitToNBitU8(reinterpret_cast<const uint16_t *>(imageData.get()),
                                          *stride, rgba8888Data.get(), dstStride, imageWidth,
                                          imageHeight, 8);
                *stride = dstStride;
                *useFloats = false;
                imageConfig = "ARGB_8888";
                imageData = rgba8888Data;
            }
            break;
        case Rgba_F16:
            if (*useFloats) {
                break;
            } else {
                int dstStride = imageWidth * 4 * (int) sizeof(uint16_t);
                std::shared_ptr<uint8_t> rgbaF16Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::Rgba8ToF16(imageData.get(), *stride,
                                  reinterpret_cast<uint16_t *>(rgbaF16Data.get()), dstStride,
                                  imageWidth, imageHeight, depth);
                *stride = dstStride;
                *useFloats = true;
                imageConfig = "RGBA_F16";
                imageData = rgbaF16Data;
            }
            break;
        case Rgb_565:
            if (*useFloats) {
                int dstStride = imageWidth * (int) sizeof(uint16_t);
                std::shared_ptr<uint8_t> rgb565Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::RGBAF16To565(reinterpret_cast<const uint16_t *>(imageData.get()), *stride,
                                    reinterpret_cast<uint16_t *>(rgb565Data.get()), dstStride,
                                    imageWidth, imageHeight);
                *stride = dstStride;
                *useFloats = false;
                imageConfig = "RGB_565";
                imageData = rgb565Data;
                break;
            } else {
                int dstStride = imageWidth * (int) sizeof(uint16_t);
                std::shared_ptr<uint8_t> rgb565Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::Rgba8To565(imageData.get(), *stride,
                                  reinterpret_cast<uint16_t *>(rgb565Data.get()), dstStride,
                                  imageWidth, imageHeight, depth);
                *stride = dstStride;
                *useFloats = false;
                imageConfig = "RGB_565";
                imageData = rgb565Data;
            }
            break;
        case Rgba_1010102:
            if (*useFloats) {
                int dstStride = imageWidth * 4 * (int) sizeof(uint8_t);
                std::shared_ptr<uint8_t> rgba1010102Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::F16ToRGBA1010102(reinterpret_cast<const uint16_t *>(imageData.get()),
                                        *stride,
                                        reinterpret_cast<uint8_t *>(rgba1010102Data.get()),
                                        dstStride,
                                        imageWidth, imageHeight);
                *stride = dstStride;
                *useFloats = false;
                imageConfig = "RGBA_1010102";
                imageData = rgba1010102Data;
                break;
            } else {
                int dstStride = imageWidth * 4 * (int) sizeof(uint8_t);
                std::shared_ptr<uint8_t> rgba1010102Data(
                        static_cast<uint8_t *>(malloc(dstStride * imageHeight)),
                        [](uint8_t *f) { free(f); });
                coder::Rgba8ToRGBA1010102(reinterpret_cast<const uint8_t *>(imageData.get()),
                                          *stride,
                                          reinterpret_cast<uint8_t *>(rgba1010102Data.get()),
                                          dstStride,
                                          imageWidth, imageHeight);
                *stride = dstStride;
                *useFloats = false;
                imageConfig = "RGBA_1010102";
                imageData = rgba1010102Data;
                break;
            }
            break;
        case Hardware: {
            if (!loadAHardwareBuffersAPI()) {
                return;
            }
            AHardwareBuffer_Desc bufferDesc = {};
            memset(&bufferDesc, 0, sizeof(AHardwareBuffer_Desc));
            bufferDesc.width = imageWidth;
            bufferDesc.height = imageHeight;
            bufferDesc.layers = 1;
            bufferDesc.format = (*useFloats) ? AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT
                                             : AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
            bufferDesc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;

            AHardwareBuffer *hardwareBuffer = nullptr;

            int status = AHardwareBuffer_allocate_compat(&bufferDesc, &hardwareBuffer);
            if (status != 0) {
                return;
            }
            ARect rect = {0, 0, imageWidth, imageHeight};
            uint8_t *buffer;
            int fence = 1;
            status = AHardwareBuffer_lock_compat(hardwareBuffer,
                                                 AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, fence,
                                                 &rect, reinterpret_cast<void **>(&buffer));
            if (status != 0) {
                AHardwareBuffer_release_compat(hardwareBuffer);
                return;
            }

            AHardwareBuffer_describe_compat(hardwareBuffer, &bufferDesc);

            int pixelSize = (*useFloats) ? sizeof(uint16_t) : sizeof(uint8_t);
            coder::CopyUnalignedRGBA(imageData.get(), *stride, buffer,
                                     (int)bufferDesc.stride * 4 * pixelSize,
                                     (int)bufferDesc.width,
                                     (int)bufferDesc.height,
                                     (*useFloats) ? sizeof(uint16_t) : sizeof(uint8_t));

            status = AHardwareBuffer_unlock_compat(hardwareBuffer, &fence);
            if (status != 0) {
                AHardwareBuffer_release_compat(hardwareBuffer);
                return;
            }

            jobject buf = AHardwareBuffer_toHardwareBuffer_compat(env, hardwareBuffer);

            AHardwareBuffer_release_compat(hardwareBuffer);

            *hwBuffer = buf;
            imageConfig = "HARDWARE";
        }
            break;
        default:
            break;
    }
}