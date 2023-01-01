package com.radzivon.bartoshyk.avif.coder

import androidx.annotation.Keep

@Keep
class HeifCantScaleException: Exception("HEIF wasn't able to scale image")