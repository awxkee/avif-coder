/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 01/01/2023
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

package com.github.awxkee.avifcoil

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.drawable.BitmapDrawable
import android.os.Build
import android.util.Size
import coil.ImageLoader
import coil.decode.DecodeResult
import coil.decode.Decoder
import coil.fetch.SourceResult
import coil.request.Options
import coil.size.Scale
import coil.size.pxOrElse
import com.radzivon.bartoshyk.avif.coder.HeifCoder
import com.radzivon.bartoshyk.avif.coder.PreferredColorConfig
import com.radzivon.bartoshyk.avif.coder.ScaleMode
import kotlinx.coroutines.runInterruptible
import okio.ByteString.Companion.encodeUtf8

class HeifDecoder(
    private val context: Context?,
    private val source: SourceResult,
    private val options: Options,
    private val imageLoader: ImageLoader,
) : Decoder {

    private val coder = HeifCoder()

    override suspend fun decode(): DecodeResult? = runInterruptible {
        // ColorSpace is preferred to be ignored due to lib is trying to handle all color profile by itself
        val sourceData = source.source.source().readByteArray()

        var mPreferredColorConfig: PreferredColorConfig = when (options.config) {
            Bitmap.Config.ALPHA_8 -> PreferredColorConfig.RGBA_8888
            Bitmap.Config.RGB_565 -> if (options.allowRgb565) PreferredColorConfig.RGB_565 else PreferredColorConfig.DEFAULT
            Bitmap.Config.ARGB_8888 -> PreferredColorConfig.RGBA_8888
            else -> PreferredColorConfig.DEFAULT
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && options.config == Bitmap.Config.RGBA_F16) {
            mPreferredColorConfig = PreferredColorConfig.RGBA_F16
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && options.config == Bitmap.Config.HARDWARE) {
            mPreferredColorConfig = PreferredColorConfig.HARDWARE
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU && options.config == Bitmap.Config.RGBA_1010102) {
            mPreferredColorConfig = PreferredColorConfig.RGBA_1010102
        }

        if (options.size == coil.size.Size.ORIGINAL) {
            val originalImage =
                coder.decode(
                    sourceData,
                    preferredColorConfig = mPreferredColorConfig
                )
            return@runInterruptible DecodeResult(
                BitmapDrawable(
                    options.context.resources,
                    originalImage
                ), false
            )
        }

        val dstWidth = options.size.width.pxOrElse { 0 }
        val dstHeight = options.size.height.pxOrElse { 0 }
        val scaleMode = when (options.scale) {
            Scale.FILL -> ScaleMode.FILL
            Scale.FIT -> ScaleMode.FIT
        }

        val originalImage =
            coder.decodeSampled(
                sourceData,
                dstWidth,
                dstHeight,
                preferredColorConfig = mPreferredColorConfig,
                scaleMode,
            )
        return@runInterruptible DecodeResult(
            BitmapDrawable(
                options.context.resources,
                originalImage
            ), true
        )
    }

    /**
     * @param context is preferred to be set when displaying an HDR content to apply Vulkan shaders
     */
    class Factory(private val context: Context?) : Decoder.Factory {
        override fun create(
            result: SourceResult,
            options: Options,
            imageLoader: ImageLoader
        ): Decoder? {
            return if (AVAILABLE_BRANDS.any {
                    result.source.source().rangeEquals(4, it)
                }) HeifDecoder(context, result, options, imageLoader) else null
        }

        companion object {
            private val MIF = "ftypmif1".encodeUtf8()
            private val MSF = "ftypmsf1".encodeUtf8()
            private val HEIC = "ftypheic".encodeUtf8()
            private val HEIX = "ftypheix".encodeUtf8()
            private val HEVC = "ftyphevc".encodeUtf8()
            private val HEVX = "ftyphevx".encodeUtf8()
            private val AVIF = "ftypavif".encodeUtf8()
            private val AVIS = "ftypavis".encodeUtf8()

            private val AVAILABLE_BRANDS = listOf(MIF, MSF, HEIC, HEIX, HEVC, HEVX, AVIF, AVIS)
        }
    }

}
