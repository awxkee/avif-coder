package com.radzivon.bartoshyk.avif.coder

import android.graphics.Bitmap

class HeifCoder {

    /**
     * A native method that is implemented by the 'coder' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    fun isAvif(byteArray: ByteArray): Boolean {
        return isAvifImageImpl(byteArray)
    }

    fun isHeif(byteArray: ByteArray): Boolean {
        return isHeifImageImpl(byteArray)
    }

    fun isSupportedImage(byteArray: ByteArray): Boolean {
        return isSupportedImageImpl(byteArray)
    }

    fun decode(byteArray: ByteArray): Bitmap {
        return decodeImpl(byteArray)
    }

    fun encodeAvif(bitmap: Bitmap): ByteArray {
        return encodeAvifImpl(bitmap)
    }

    fun encodeHeic(bitmap: Bitmap): ByteArray {
        return encodeHeicImpl(bitmap)
    }

    private external fun isHeifImageImpl(byteArray: ByteArray): Boolean
    private external fun isAvifImageImpl(byteArray: ByteArray): Boolean
    private external fun isSupportedImageImpl(byteArray: ByteArray): Boolean
    private external fun decodeImpl(byteArray: ByteArray): Bitmap
    private external fun encodeAvifImpl(bitmap: Bitmap): ByteArray
    private external fun encodeHeicImpl(bitmap: Bitmap): ByteArray

    companion object {
        init {
            System.loadLibrary("coder")
        }
    }
}