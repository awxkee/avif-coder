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

/**
 * Enum representing speed values from 0 to 10 (slowest-fastest).
 * Default is 10.
 * https://github.com/AOMediaCodec/libavif/blob/main/doc/avifenc.1.md
 */
enum class AvifSpeed(internal val value: Int) {
    ZERO(0),  // Speed 0
    ONE(1),   // Speed 1
    TWO(2),   // Speed 2
    THREE(3), // Speed 3
    FOUR(4),  // Speed 4
    FIVE(5),  // Speed 5
    SIX(6),   // Speed 6
    SEVEN(7), // Speed 7
    EIGHT(8), // Speed 8
    NINE(9),  // Speed 9
    TEN(10)   // Speed 10
}