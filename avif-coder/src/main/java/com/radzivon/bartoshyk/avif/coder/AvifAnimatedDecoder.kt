/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 13/10/2024
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

package com.radzivon.bartoshyk.avif.coder

import android.annotation.SuppressLint
import android.graphics.Bitmap
import android.os.Build
import android.util.Size
import androidx.annotation.Keep
import java.io.Closeable
import java.nio.ByteBuffer

/**
 * Class that manages animation avif decoding.
 *
 * If there are many opened animations/their size is huge, consider manual releasing of resources
 * using [Closeable] to avoid OOM.
 *
 * @throws Exception - All functions in this class may throw if something goes wrong
 */
@Keep
@SuppressLint("ObsoleteSdkInt")
class AvifAnimatedDecoder : Closeable {

    init {
        if (Build.VERSION.SDK_INT >= 24) {
            System.loadLibrary("coder")
        }
    }

    constructor(source: ByteArray) {
        nativeController = createControllerFromByteArray(source)
    }

    constructor(source: ByteBuffer) {
        nativeController = createControllerFromByteBuffer(source)
    }

    var toneMapper: ToneMapper = ToneMapper.REC2408

    private var nativeController: Long = -1
    private val lock = Any()

    fun getScaledFrame(
        frame: Int, scaledWidth: Int,
        scaledHeight: Int,
        preferredColorConfig: PreferredColorConfig = PreferredColorConfig.DEFAULT,
        scaleMode: ScaleMode = ScaleMode.FIT,
        scaleQuality: ScalingQuality = ScalingQuality.DEFAULT,
    ): Bitmap {
        synchronized(lock) {
            if (nativeController == -1L) {
                throw IllegalStateException("Animated decoder wasn't properly initialized")
            }
            return getFrameImpl(
                nativeController,
                frame,
                scaledWidth,
                scaledHeight,
                preferredColorConfig.value,
                scaleMode.value,
                scaleQuality.level,
                toneMapper.value
            )
        }
    }

    fun getFrame(
        frame: Int,
        preferredColorConfig: PreferredColorConfig = PreferredColorConfig.DEFAULT,
    ): Bitmap {
        return getScaledFrame(
            frame,
            0,
            0,
            preferredColorConfig,
            ScaleMode.FIT,
            ScalingQuality.DEFAULT
        )
    }

    fun getImageSize(): Size {
        synchronized(lock) {
            if (nativeController == -1L) {
                throw IllegalStateException("Animated decoder wasn't properly initialized")
            }
            return getSizeImpl(nativeController)
        }
    }

    fun getFramesCount(): Int {
        synchronized(lock) {
            if (nativeController == -1L) {
                throw IllegalStateException("Animated decoder wasn't properly initialized")
            }
            return getFramesCount(nativeController)
        }
    }

    fun getLoopsCount(): Int {
        synchronized(lock) {
            if (nativeController == -1L) {
                throw IllegalStateException("Animated decoder wasn't properly initialized")
            }
            return getLoopsCountImpl(nativeController)
        }
    }

    fun getTotalDuration(): Int {
        synchronized(lock) {
            if (nativeController == -1L) {
                throw IllegalStateException("Animated decoder wasn't properly initialized")
            }
            return getTotalDurationImpl(nativeController)
        }
    }

    fun getFrameDuration(frame: Int): Int {
        synchronized(lock) {
            if (nativeController == -1L) {
                throw IllegalStateException("Animated decoder wasn't properly initialized")
            }
            return getFrameDurationImpl(nativeController, frame)
        }
    }

    protected fun finalize() {
        synchronized(lock) {
            if (nativeController != -1L) {
                destroy(nativeController)
                nativeController = -1L
            }
        }
    }

    override fun close() {
        synchronized(lock) {
            if (nativeController != -1L) {
                destroy(nativeController)
                nativeController = -1L
            }
        }
    }

    private external fun destroy(ptr: Long)
    private external fun createControllerFromByteArray(byteArray: ByteArray): Long
    private external fun createControllerFromByteBuffer(byteBuffer: ByteBuffer): Long
    private external fun getFramesCount(ptr: Long): Int
    private external fun getLoopsCountImpl(ptr: Long): Int
    private external fun getTotalDurationImpl(ptr: Long): Int
    private external fun getFrameDurationImpl(ptr: Long, frame: Int): Int
    private external fun getSizeImpl(ptr: Long): Size
    private external fun getFrameImpl(
        ptr: Long,
        frame: Int, scaledWidth: Int,
        scaledHeight: Int,
        preferredColorConfig: Int,
        scaleMode: Int,
        scaleQuality: Int,
        toneMapper: Int,
    ): Bitmap

}