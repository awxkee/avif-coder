package com.radzivon.bartoshyk.avif.coder

class UnsupportedImageFormatException :
    Exception("Currently support only RGBA_8888, RGB_565, RGBA_F16 image format") {
}