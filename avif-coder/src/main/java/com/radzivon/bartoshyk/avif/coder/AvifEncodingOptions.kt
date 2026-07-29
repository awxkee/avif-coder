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

import androidx.annotation.IntRange
import androidx.annotation.Keep

@Keep
data class AvifEncodingOptions @JvmOverloads constructor(
    @param:IntRange(from = 0, to = 100)
    val quality: Int = 80,
    val preciseMode: PreciseMode = PreciseMode.LOSSY,
    val chromaSubsampling: AvifChromaSubsampling = AvifChromaSubsampling.AUTO,
    val avKind: AvKind = AvKind.AV1,
    val speed: AvSpeed = AvSpeed.FAST,
    // Better screenshots compression for higher encoding times
    @get:Keep
    val screenContentCoding: Boolean = false,
) {
    init {
        require(quality in 0..100) {
            "Quality should be in 0..100 range"
        }
    }

    @Keep
    fun getQualityValue(): Int = quality

    @Keep
    fun isLossless(): Boolean = preciseMode == PreciseMode.LOSSLESS

    @Keep
    fun getChromaSubsamplingValue(): Int = chromaSubsampling.value

    @Keep
    fun useAv2(): Boolean = avKind == AvKind.AV2

    @Keep
    fun getSpeedValue(): Int = speed.value
}
