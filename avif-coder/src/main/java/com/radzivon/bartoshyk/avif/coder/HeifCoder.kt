package com.radzivon.bartoshyk.avif.coder

import android.graphics.Bitmap

class HeifCoder {

    /**
     * A native method that is implemented by the 'coder' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    fun decodeAvif(byteArray: ByteArray): Bitmap {
        return decodeAvifImpl(byteArray)
    }

    fun encodeAvif(bitmap: Bitmap): ByteArray {
        return encodeAvifImpl(bitmap)
    }

    fun encodeHeic(bitmap: Bitmap): ByteArray {
        return encodeHeicImpl(bitmap)
    }

    private external fun decodeAvifImpl(byteArray: ByteArray): Bitmap
    private external fun encodeAvifImpl(bitmap: Bitmap): ByteArray
    private external fun encodeHeicImpl(bitmap: Bitmap): ByteArray

    companion object {
        init {
            System.loadLibrary("coder")
        }
    }
}