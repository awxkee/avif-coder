/*
 * MIT License
 *
 * Copyright (c) 2026 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
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
 */

package com.radzivon.bartoshyk.avif.coder

import androidx.annotation.Keep

@Keep
data class HevcEncodingOptions @JvmOverloads constructor(
    val preciseMode: PreciseMode = PreciseMode.LOSSY,
    val quality: HeifQualityArgument = HeifQualityArg.Quality(100),
    val chromaSubsampling: HeicChromaSubsampling = HeicChromaSubsampling.YUV420,
    val speed: HevcSpeed = HevcSpeed.FAST,
    /// Most of HEVC decoders DO NOT support RExt SCC feature, so use with care.
    @get:Keep
    val screenContentCoding: Boolean = false,
    /// Enables implicit RDPCM for lossless HEVC encoding.
    @get:Keep
    val rdpcm: Boolean = false,
) {
    @Keep
    fun getQualityValue(): Int = quality.getRequiredQuality()

    @Keep
    fun isLossless(): Boolean = preciseMode == PreciseMode.LOSSLESS

    @Keep
    fun getChromaSubsamplingValue(): Int = chromaSubsampling.value

    @Keep
    fun getSpeedValue(): Int = speed.value
}
