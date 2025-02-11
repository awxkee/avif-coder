/*
 * MIT License
 *
 * Copyright (c) 2025 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 11/2/2025
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

import androidx.annotation.IntRange

sealed interface HeifQualityArgument {
    fun getRequiredQuality(): Int
    fun getRequiredPreset(): HeifPreset
    fun getRequiredCrf(): Int
    fun isCrfMode(): Boolean
}

sealed class HeifQualityArg: HeifQualityArgument {
    data class Quality(@IntRange(from = 0, to = 100) val qual: Int):HeifQualityArgument {
        init {
            require(qual in 0..100) {
                throw IllegalStateException("Quality should be in 0..100 range")
            }
        }

        override fun getRequiredQuality(): Int {
            return qual
        }

        override fun getRequiredPreset(): HeifPreset {
            return HeifPreset.ULTRAFAST
        }

        override fun getRequiredCrf(): Int {
            return 40
        }

        override fun isCrfMode(): Boolean {
            return false
        }
    }
    data class Crf(@IntRange(from = 0, to = 51) val crf: Int = 40, val preset: HeifPreset = HeifPreset.ULTRAFAST): HeifQualityArgument {
        init {
            require(crf in 0..51) {
                throw IllegalStateException("CRF should be in 0..51 range")
            }
        }

        override fun getRequiredQuality(): Int {
            return 100
        }

        override fun getRequiredPreset(): HeifPreset {
            return preset
        }

        override fun getRequiredCrf(): Int {
            return crf
        }

        override fun isCrfMode(): Boolean {
            return true
        }
    }
}