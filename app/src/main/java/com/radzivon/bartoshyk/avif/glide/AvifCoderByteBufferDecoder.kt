/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 23/09/2023
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

package com.radzivon.bartoshyk.avif.glide

import android.content.Context
import android.graphics.Bitmap
import android.os.Build
import com.bumptech.glide.load.DecodeFormat
import com.bumptech.glide.load.Options
import com.bumptech.glide.load.ResourceDecoder
import com.bumptech.glide.load.engine.Resource
import com.bumptech.glide.load.engine.bitmap_recycle.BitmapPool
import com.bumptech.glide.load.resource.bitmap.BitmapResource
import com.bumptech.glide.load.resource.bitmap.Downsampler
import com.bumptech.glide.request.target.Target
import com.radzivon.bartoshyk.avif.coder.HeifCoder
import com.radzivon.bartoshyk.avif.coder.PreferredColorConfig
import com.radzivon.bartoshyk.avif.coder.ScaleMode
import com.radzivon.bartoshyk.avif.coder.ScalingQuality
import java.nio.ByteBuffer

class AvifCoderByteBufferDecoder(private val context: Context, private val bitmapPool: BitmapPool) :
    ResourceDecoder<ByteBuffer, Bitmap> {

    private val coder = HeifCoder()

    override fun handles(source: ByteBuffer, options: Options): Boolean {
        return coder.isSupportedImage(source)
    }

    override fun decode(
        source: ByteBuffer,
        width: Int,
        height: Int,
        options: Options
    ): Resource<Bitmap>? {
        val src = refactorToDirect(source)
        val allowedHardwareConfig = options[Downsampler.ALLOW_HARDWARE_CONFIG] ?: false
        Downsampler.FIX_BITMAP_SIZE_TO_REQUESTED_DIMENSIONS

        var idealWidth = width
        if (idealWidth == Target.SIZE_ORIGINAL) {
            idealWidth = -1
        }
        var idealHeight = height
        if (idealHeight == Target.SIZE_ORIGINAL) {
            idealHeight = -1
        }

        val preferredColorConfig: PreferredColorConfig = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && allowedHardwareConfig) {
                PreferredColorConfig.HARDWARE
            } else {
                if (options[Downsampler.DECODE_FORMAT] === DecodeFormat.PREFER_RGB_565) {
                    PreferredColorConfig.RGB_565
                } else {
                    PreferredColorConfig.DEFAULT
                }
            }

        val bitmap =
            coder.decodeSampled(src, idealWidth, idealHeight, preferredColorConfig, ScaleMode.FIT, ScalingQuality.HIGH)

        return BitmapResource.obtain(bitmap, bitmapPool)
    }

    private fun refactorToDirect(source: ByteBuffer): ByteBuffer {
        if (source.isDirect) {
            return source
        }
        val sourceCopy = ByteBuffer.allocateDirect(source.remaining())
        sourceCopy.put(source)
        sourceCopy.flip()
        return sourceCopy
    }

}