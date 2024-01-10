/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 16/9/2023
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
#include <mutex>
#include "imagebits/RGBAlpha.h"

using namespace std;

namespace coder {
    void
    ReformatColorConfig(JNIEnv *env, vector <uint8_t> &imageData, string &imageConfig,
                        PreferredColorConfig preferredColorConfig, int depth,
                        int imageWidth, int imageHeight, int *stride, bool *useFloats,
                        jobject *hwBuffer, bool alphaPremultiplied) {
        *hwBuffer = nullptr;
        switch (preferredColorConfig) {
            case Rgba_8888:
                if (*useFloats) {
                    int lineWidth = imageWidth * 4 * (int) sizeof(uint8_t);
                    int alignment = 64;
                    int padding = (alignment - (lineWidth % alignment)) % alignment;
                    int dstStride = lineWidth + padding;
                    vector<uint8_t> rgba8888Data(dstStride * imageHeight);
                    coder::RGBAF16BitToNBitU8(reinterpret_cast<const uint16_t *>(imageData.data()),
                                              *stride, rgba8888Data.data(), dstStride, imageWidth,
                                              imageHeight, 8);
                    *stride = dstStride;
                    *useFloats = false;
                    imageConfig = "ARGB_8888";
                    imageData = rgba8888Data;
                }

                if (!alphaPremultiplied) {
                    coder::PremultiplyRGBA(imageData.data(), *stride,
                                           imageData.data(), *stride,
                                           imageWidth,
                                           imageHeight);
                }

                break;
            case Rgba_F16:
                if (*useFloats) {
                    break;
                } else {
                    int lineWidth = imageWidth * 4 * (int) sizeof(uint16_t);
                    int alignment = 64;
                    int padding = (alignment - (lineWidth % alignment)) % alignment;
                    int dstStride = lineWidth + padding;
                    vector<uint8_t> rgbaF16Data(dstStride * imageHeight);
                    coder::Rgba8ToF16(imageData.data(), *stride,
                                      reinterpret_cast<uint16_t *>(rgbaF16Data.data()), dstStride,
                                      imageWidth, imageHeight, !alphaPremultiplied);
                    *stride = dstStride;
                    *useFloats = true;
                    imageConfig = "RGBA_F16";
                    imageData = rgbaF16Data;
                }
                break;
            case Rgb_565:
                if (*useFloats) {
                    int lineWidth = imageWidth * (int) sizeof(uint16_t);
                    int alignment = 64;
                    int padding = (alignment - (lineWidth % alignment)) % alignment;
                    int dstStride = lineWidth + padding;
                    vector<uint8_t> rgb565Data(dstStride * imageHeight);
                    coder::RGBAF16To565(reinterpret_cast<const uint16_t *>(imageData.data()),
                                        *stride,
                                        reinterpret_cast<uint16_t *>(rgb565Data.data()), dstStride,
                                        imageWidth, imageHeight);
                    *stride = dstStride;
                    *useFloats = false;
                    imageConfig = "RGB_565";
                    imageData = rgb565Data;
                    break;
                } else {
                    int lineWidth = imageWidth * (int) sizeof(uint16_t);
                    int alignment = 64;
                    int padding = (alignment - (lineWidth % alignment)) % alignment;
                    int dstStride = lineWidth + padding;
                    vector<uint8_t> rgb565Data(dstStride * imageHeight);
                    coder::Rgba8To565(imageData.data(), *stride,
                                      reinterpret_cast<uint16_t *>(rgb565Data.data()), dstStride,
                                      imageWidth, imageHeight,
                                      !alphaPremultiplied);
                    *stride = dstStride;
                    *useFloats = false;
                    imageConfig = "RGB_565";
                    imageData = rgb565Data;
                }
                break;
            case Rgba_1010102:
                if (*useFloats) {
                    int lineWidth = imageWidth * (int) sizeof(uint32_t);
                    int alignment = 64;
                    int padding = (alignment - (lineWidth % alignment)) % alignment;
                    int dstStride = lineWidth + padding;
                    vector<uint8_t> rgba1010102Data(dstStride * imageHeight);
                    coder::F16ToRGBA1010102(reinterpret_cast<const uint16_t *>(imageData.data()),
                                            *stride,
                                            reinterpret_cast<uint8_t *>(rgba1010102Data.data()),
                                            dstStride,
                                            imageWidth, imageHeight);
                    *stride = dstStride;
                    *useFloats = false;
                    imageConfig = "RGBA_1010102";
                    imageData = rgba1010102Data;
                    break;
                } else {
                    int lineWidth = imageWidth * (int) sizeof(uint32_t);
                    int alignment = 64;
                    int padding = (alignment - (lineWidth % alignment)) % alignment;
                    int dstStride = lineWidth + padding;
                    vector<uint8_t> rgba1010102Data(dstStride * imageHeight);
                    coder::Rgba8ToRGBA1010102(reinterpret_cast<const uint8_t *>(imageData.data()),
                                              *stride,
                                              reinterpret_cast<uint8_t *>(rgba1010102Data.data()),
                                              dstStride,
                                              imageWidth, imageHeight,
                                              !alphaPremultiplied);
                    *stride = dstStride;
                    *useFloats = false;
                    imageConfig = "RGBA_1010102";
                    imageData = rgba1010102Data;
                    break;
                }
                break;
            case Hardware: {
                if (!loadAHardwareBuffersAPI()) {
                    string err = "Cannot load hardware buffers API";
                    throw ReformatBitmapError(err);
                }
                AHardwareBuffer_Desc bufferDesc = {0};
                bufferDesc.width = imageWidth;
                bufferDesc.height = imageHeight;
                bufferDesc.layers = 1;
                bufferDesc.format = (*useFloats) ? AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT
                                                 : AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;

                bufferDesc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;

                if (!*useFloats && !alphaPremultiplied) {
                    coder::PremultiplyRGBA(imageData.data(), *stride,
                                           imageData.data(), *stride,
                                           imageWidth,
                                           imageHeight);
                }

                AHardwareBuffer *hardwareBuffer = nullptr;

                int status = AHardwareBuffer_allocate_compat(&bufferDesc, &hardwareBuffer);
                if (status != 0) {
                    string err = "Cannot allocate Hardware Buffer";
                    throw ReformatBitmapError(err);
                }
                ARect rect = {0, 0, imageWidth, imageHeight};
                uint8_t *buffer = nullptr;

                status = AHardwareBuffer_lock_compat(hardwareBuffer,
                                                     AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1,
                                                     &rect, reinterpret_cast<void **>(&buffer));
                if (status != 0) {
                    AHardwareBuffer_release_compat(hardwareBuffer);
                    string err = "Cannot lock hardware buffer for write";
                    throw ReformatBitmapError(err);
                }

                AHardwareBuffer_describe_compat(hardwareBuffer, &bufferDesc);

                int pixelSize = (*useFloats) ? sizeof(uint16_t) : sizeof(uint8_t);
                CopyUnaligned(imageData.data(), *stride, buffer,
                                  (int) bufferDesc.stride * 4 * pixelSize,
                                  (int) bufferDesc.width * 4,
                                  (int) bufferDesc.height,
                                  pixelSize);

                status = AHardwareBuffer_unlock_compat(hardwareBuffer, nullptr);
                if (status != 0) {
                    AHardwareBuffer_release_compat(hardwareBuffer);
                    string err = "Cannot unlock hardware buffer";
                    throw ReformatBitmapError(err);
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
}