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
import android.content.Context
import android.content.res.AssetManager
import android.graphics.Bitmap
import android.os.Build
import android.util.Size
import androidx.annotation.Keep
import java.nio.ByteBuffer

@Keep
class HeifCoder(
    private val toneMapper: ToneMapper = ToneMapper.REC2408
) {

    fun isAvif(byteArray: ByteArray): Boolean {
        return isAvifImageImpl(byteArray)
    }

    fun isHeif(byteArray: ByteArray): Boolean {
        return isHeifImageImpl(byteArray)
    }

    fun isSupportedImage(byteArray: ByteArray): Boolean {
        return isSupportedImageImpl(byteArray)
    }

    fun isSupportedImage(byteBuffer: ByteBuffer): Boolean {
        return isSupportedImageImplBB(byteBuffer)
    }

    fun getSize(bytes: ByteArray): Size? {
        return getSizeImpl(bytes)
    }

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
            toneMapper.value,
            ScalingQuality.DEFAULT.level,
        )
    }

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
            toneMapper.value,
            scaleQuality.level,
        )
    }

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
            toneMapper.value,
            scaleQuality.level,
        )
    }

    /**
     * Encodes an avif image
     *
     * @param quality must be in range 0..100
     * @param speed - see [AvifSpeed] for more info
     * @param preciseMode - LOSSY or LOSELESS compression mode
     * @param surfaceMode - see [AvifSurfaceMode] for more info
     *
     * @throws IllegalArgumentException if image size is not even
     */
    fun encodeAvif(
        bitmap: Bitmap,
        quality: Int = 80,
        speed: AvifSpeed = AvifSpeed.TEN,
        preciseMode: PreciseMode = PreciseMode.LOSSY,
        surfaceMode: AvifSurfaceMode = AvifSurfaceMode.AUTO,
        avifChromaSubsampling: AvifChromaSubsampling = AvifChromaSubsampling.AUTO,
    ): ByteArray {
        require(quality in 0..100) {
            throw IllegalStateException("Quality should be in 0..100 range")
        }
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            encodeAvifImpl(
                bitmap,
                quality,
                bitmap.colorSpace?.dataSpace ?: -1,
                preciseMode.value,
                surfaceMode.value,
                speed = speed.value,
                avifChromaSubsampling.value
            )
        } else {
            encodeAvifImpl(
                bitmap,
                quality,
                -1,
                preciseMode.value,
                surfaceMode.value,
                speed = speed.value,
                avifChromaSubsampling.value
            )
        }
    }

    fun encodeHeic(
        bitmap: Bitmap,
        quality: Int = 80,
        preciseMode: PreciseMode = PreciseMode.LOSSY
    ): ByteArray {
        require(quality in 0..100) {
            throw IllegalStateException("Quality should be in 0..100 range")
        }
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            encodeHeicImpl(bitmap, quality, bitmap.colorSpace?.dataSpace ?: -1, preciseMode.value)
        } else {
            encodeHeicImpl(bitmap, quality, -1, preciseMode.value)
        }
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
        toneMapper: Int,
        scaleQuality: Int,
    ): Bitmap

    private external fun decodeByteBufferImpl(
        byteArray: ByteBuffer,
        scaledWidth: Int,
        scaledHeight: Int,
        clrConfig: Int,
        scaleMode: Int,
        toneMapper: Int,
        scaleQuality: Int,
    ): Bitmap

    private external fun encodeAvifImpl(
        bitmap: Bitmap,
        quality: Int,
        dataSpace: Int,
        qualityMode: Int,
        surfaceMode: Int,
        speed: Int,
        chromaSubsampling: Int,
    ): ByteArray

    private external fun encodeHeicImpl(
        bitmap: Bitmap,
        quality: Int,
        dataSpace: Int,
        qualityMode: Int
    ): ByteArray

    @SuppressLint("ObsoleteSdkInt")
    companion object {
        init {
            if (Build.VERSION.SDK_INT >= 24) {
                System.loadLibrary("coder")
            }
        }
    }
}
