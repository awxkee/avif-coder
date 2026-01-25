/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 23/9/2023
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

@file:Suppress("unused")

package com.radzivon.bartoshyk.avif.coder

import android.annotation.SuppressLint
import android.graphics.Bitmap
import android.os.Build
import android.util.Size
import androidx.annotation.Keep
import java.nio.ByteBuffer

/**
 * Main decoder class for AVIF and HEIF/HEIC images on Android.
 * 
 * This class provides a simple interface to decode AVIF (AV1) and HEIC/HEIF (HEVC) images,
 * with support for HDR images, wide color gamut, and ICC profiles.
 * 
 * @sample
 * ```
 * val coder = HeifCoder()
 * val bitmap = coder.decode(imageBytes)
 * ```
 */
@Keep
class HeifCoder {

    /**
     * Checks if the provided byte array contains an AVIF image.
     * 
     * @param byteArray The image data to check
     * @return true if the data represents an AVIF image, false otherwise
     */
    fun isAvif(byteArray: ByteArray): Boolean {
        return isAvifImageImpl(byteArray)
    }

    /**
     * Checks if the provided byte array contains a HEIF/HEIC image.
     * 
     * @param byteArray The image data to check
     * @return true if the data represents a HEIF/HEIC image, false otherwise
     */
    fun isHeif(byteArray: ByteArray): Boolean {
        return isHeifImageImpl(byteArray)
    }

    /**
     * Checks if the provided byte array contains a supported image format (AVIF or HEIF/HEIC).
     * 
     * @param byteArray The image data to check
     * @return true if the data represents a supported image format, false otherwise
     */
    fun isSupportedImage(byteArray: ByteArray): Boolean {
        return isSupportedImageImpl(byteArray)
    }

    /**
     * Checks if the provided ByteBuffer contains a supported image format (AVIF or HEIF/HEIC).
     * 
     * @param byteBuffer The image data to check (must be a direct ByteBuffer)
     * @return true if the data represents a supported image format, false otherwise
     * @throws Exception if the ByteBuffer is not a direct buffer
     */
    fun isSupportedImage(byteBuffer: ByteBuffer): Boolean {
        return isSupportedImageImplBB(byteBuffer)
    }

    /**
     * Gets the dimensions of the image without fully decoding it.
     * 
     * This method is efficient as it only reads the image header.
     * 
     * @param bytes The image data
     * @return The image size as [Size], or null if the image cannot be read or is invalid
     */
    fun getSize(bytes: ByteArray): Size? {
        return getSizeImpl(bytes)
    }

    /**
     * Decodes an AVIF or HEIF/HEIC image to a Bitmap at its original size.
     * 
     * @param byteArray The image data to decode
     * @param preferredColorConfig The preferred color configuration for the output bitmap
     * @return The decoded Bitmap
     * @throws Exception if decoding fails or the image format is not supported
     */
    fun decode(
        byteArray: ByteArray,
        preferredColorConfig: PreferredColorConfig = PreferredColorConfig.DEFAULT
    ): Bitmap {
        return decodeImpl(
            byteArray,
            0,
            0,
            preferredColorConfig.value,
            ScaleMode.FIT.value,
            ScalingQuality.DEFAULT.level,
        )
    }

    /**
     * Decodes an AVIF or HEIF/HEIC image to a Bitmap with scaling.
     * 
     * @param byteArray The image data to decode
     * @param scaledWidth The target width (0 means no scaling)
     * @param scaledHeight The target height (0 means no scaling)
     * @param preferredColorConfig The preferred color configuration for the output bitmap
     * @param scaleMode The scaling mode (FIT or FILL)
     * @param scaleQuality The quality of scaling
     * @return The decoded and scaled Bitmap
     * @throws Exception if decoding fails, the image format is not supported, or dimensions are invalid
     */
    fun decodeSampled(
        byteArray: ByteArray,
        scaledWidth: Int,
        scaledHeight: Int,
        preferredColorConfig: PreferredColorConfig = PreferredColorConfig.DEFAULT,
        scaleMode: ScaleMode = ScaleMode.FIT,
        scaleQuality: ScalingQuality = ScalingQuality.DEFAULT,
    ): Bitmap {
        return decodeImpl(
            byteArray,
            scaledWidth,
            scaledHeight,
            preferredColorConfig.value,
            scaleMode.value,
            scaleQuality.level,
        )
    }

    /**
     * Decodes an AVIF or HEIF/HEIC image from a ByteBuffer with scaling.
     * 
     * @param byteBuffer The image data to decode (must be a direct ByteBuffer)
     * @param scaledWidth The target width (0 means no scaling)
     * @param scaledHeight The target height (0 means no scaling)
     * @param preferredColorConfig The preferred color configuration for the output bitmap
     * @param scaleMode The scaling mode (FIT or FILL)
     * @param scaleQuality The quality of scaling
     * @return The decoded and scaled Bitmap
     * @throws Exception if decoding fails, the ByteBuffer is not direct, or dimensions are invalid
     */
    fun decodeSampled(
        byteBuffer: ByteBuffer,
        scaledWidth: Int,
        scaledHeight: Int,
        preferredColorConfig: PreferredColorConfig = PreferredColorConfig.DEFAULT,
        scaleMode: ScaleMode = ScaleMode.FIT,
        scaleQuality: ScalingQuality = ScalingQuality.DEFAULT,
    ): Bitmap {
        return decodeByteBufferImpl(
            byteBuffer,
            scaledWidth,
            scaledHeight,
            preferredColorConfig.value,
            scaleMode.value,
            scaleQuality.level,
        )
    }

    private external fun getSizeImpl(byteArray: ByteArray): Size
    private external fun isHeifImageImpl(byteArray: ByteArray): Boolean
    private external fun isAvifImageImpl(byteArray: ByteArray): Boolean
    private external fun isSupportedImageImpl(byteArray: ByteArray): Boolean
    private external fun isSupportedImageImplBB(byteBuffer: ByteBuffer): Boolean
    private external fun decodeImpl(
        byteArray: ByteArray,
        scaledWidth: Int,
        scaledHeight: Int,
        clrConfig: Int,
        scaleMode: Int,
        scaleQuality: Int,
    ): Bitmap

    private external fun decodeByteBufferImpl(
        byteArray: ByteBuffer,
        scaledWidth: Int,
        scaledHeight: Int,
        clrConfig: Int,
        scaleMode: Int,
        scaleQuality: Int,
    ): Bitmap

    @SuppressLint("ObsoleteSdkInt")
    companion object {
        init {
            if (Build.VERSION.SDK_INT >= 24) {
                System.loadLibrary("coder")
            }
        }
    }
}
