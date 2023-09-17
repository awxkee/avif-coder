package com.radzivon.bartoshyk.avif.glide

import android.content.Context
import android.graphics.Bitmap
import com.bumptech.glide.Glide
import com.bumptech.glide.Registry
import com.bumptech.glide.annotation.GlideModule
import com.bumptech.glide.module.LibraryGlideModule
import java.io.InputStream
import java.nio.ByteBuffer

@GlideModule
class AvifCoderGlideModule : LibraryGlideModule() {
    override fun registerComponents(context: Context, glide: Glide, registry: Registry) {
        // Add the Avif ResourceDecoders before any of the available system decoders. This ensures that
        // the integration will be preferred for Avif images.
        val byteBufferBitmapDecoder = AvifCoderByteBufferDecoder(glide.bitmapPool)
        registry.prepend(ByteBuffer::class.java, Bitmap::class.java, byteBufferBitmapDecoder)
        val streamBitmapDecoder = AvifCoderStreamDecoder(glide.bitmapPool)
        registry.prepend(InputStream::class.java, Bitmap::class.java, streamBitmapDecoder)
    }
}