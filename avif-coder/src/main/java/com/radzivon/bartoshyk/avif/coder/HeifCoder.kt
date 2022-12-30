package com.radzivon.bartoshyk.avif.coder

import android.graphics.Bitmap
import android.util.Size

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

    fun getSize(bytes: ByteArray): Size? {
        return getSizeImpl(bytes)
    }

    fun decode(byteArray: ByteArray): Bitmap {
        return decodeImpl(byteArray, 0, 0)
    }

    fun decodeSampled(byteArray: ByteArray, scaledWidth: Int, scaledHeight: Int): Bitmap {
        return decodeImpl(byteArray, scaledWidth, scaledHeight)
    }

    fun encodeAvif(bitmap: Bitmap): ByteArray {
        return encodeAvifImpl(bitmap)
    }

    fun encodeHeic(bitmap: Bitmap): ByteArray {
        return encodeHeicImpl(bitmap)
    }

    private external fun getSizeImpl(byteArray: ByteArray): Size?
    private external fun isHeifImageImpl(byteArray: ByteArray): Boolean
    private external fun isAvifImageImpl(byteArray: ByteArray): Boolean
    private external fun isSupportedImageImpl(byteArray: ByteArray): Boolean
    private external fun decodeImpl(byteArray: ByteArray, scaledWidth: Int, scaledHeight: Int): Bitmap
    private external fun encodeAvifImpl(bitmap: Bitmap): ByteArray
    private external fun encodeHeicImpl(bitmap: Bitmap): ByteArray

    companion object {
        init {
            System.loadLibrary("coder")
        }
    }
}