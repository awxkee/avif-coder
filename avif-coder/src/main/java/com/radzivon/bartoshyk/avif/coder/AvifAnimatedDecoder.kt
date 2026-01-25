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

    /**
     * Tone mapper to use for HDR images.
     * Default is [ToneMapper.REC2408].
     */
    var toneMapper: ToneMapper = ToneMapper.REC2408

    private var nativeController: Long = -1
    private val lock = Any()

    /**
     * Gets a specific frame from an animated AVIF image with scaling.
     * 
     * @param frame The frame index (0-based)
     * @param scaledWidth The target width (0 means no scaling)
     * @param scaledHeight The target height (0 means no scaling)
     * @param preferredColorConfig The preferred color configuration for the output bitmap
     * @param scaleMode The scaling mode (FIT or FILL)
     * @param scaleQuality The quality of scaling
     * @return The decoded and scaled frame as a Bitmap
     * @throws IllegalStateException if the decoder wasn't properly initialized
     * @throws Exception if decoding fails or frame index is out of bounds
     */
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

    /**
     * Gets a specific frame from an animated AVIF image at its original size.
     * 
     * @param frame The frame index (0-based)
     * @param preferredColorConfig The preferred color configuration for the output bitmap
     * @return The decoded frame as a Bitmap
     * @throws IllegalStateException if the decoder wasn't properly initialized
     * @throws Exception if decoding fails or frame index is out of bounds
     */
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

    /**
     * Gets the dimensions of the animated AVIF image.
     * 
     * @return The image size as [Size]
     * @throws IllegalStateException if the decoder wasn't properly initialized
     */
    fun getImageSize(): Size {
        synchronized(lock) {
            if (nativeController == -1L) {
                throw IllegalStateException("Animated decoder wasn't properly initialized")
            }
            return getSizeImpl(nativeController)
        }
    }

    /**
     * Gets the total number of frames in the animated AVIF image.
     * 
     * @return The number of frames
     * @throws IllegalStateException if the decoder wasn't properly initialized
     */
    fun getFramesCount(): Int {
        synchronized(lock) {
            if (nativeController == -1L) {
                throw IllegalStateException("Animated decoder wasn't properly initialized")
            }
            return getFramesCount(nativeController)
        }
    }

    /**
     * Gets the number of times the animation should loop.
     * 
     * @return The loop count (0 means infinite loop)
     * @throws IllegalStateException if the decoder wasn't properly initialized
     */
    fun getLoopsCount(): Int {
        synchronized(lock) {
            if (nativeController == -1L) {
                throw IllegalStateException("Animated decoder wasn't properly initialized")
            }
            return getLoopsCountImpl(nativeController)
        }
    }

    /**
     * Gets the total duration of the animation in milliseconds.
     * 
     * @return The total duration in milliseconds
     * @throws IllegalStateException if the decoder wasn't properly initialized
     */
    fun getTotalDuration(): Int {
        synchronized(lock) {
            if (nativeController == -1L) {
                throw IllegalStateException("Animated decoder wasn't properly initialized")
            }
            return getTotalDurationImpl(nativeController)
        }
    }

    /**
     * Gets the duration of a specific frame in milliseconds.
     * 
     * @param frame The frame index (0-based)
     * @return The frame duration in milliseconds
     * @throws IllegalStateException if the decoder wasn't properly initialized
     * @throws Exception if frame index is out of bounds
     */
    fun getFrameDuration(frame: Int): Int {
        synchronized(lock) {
            if (nativeController == -1L) {
                throw IllegalStateException("Animated decoder wasn't properly initialized")
            }
            return getFrameDurationImpl(nativeController, frame)
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