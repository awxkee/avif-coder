/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
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

package com.radzivon.bartoshyk.avif

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Paint
import android.os.Build
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import android.util.Size
import androidx.core.graphics.scale
import androidx.lifecycle.lifecycleScope
import coil.ImageLoader
import coil.load
import com.bumptech.glide.Glide
import com.bumptech.glide.load.engine.DiskCacheStrategy
import com.github.awxkee.avifcoil.HeifDecoder
import com.radzivon.bartoshyk.avif.coder.HeifCoder
import com.radzivon.bartoshyk.avif.coder.PreferredColorConfig
import com.radzivon.bartoshyk.avif.coder.ScaleMode
import com.radzivon.bartoshyk.avif.databinding.ActivityMainBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import okio.buffer
import okio.sink
import okio.source
import java.io.File
import java.io.FileOutputStream
import kotlin.system.measureTimeMillis

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Example of a call to a native method
//
        val coder = HeifCoder()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            val buffer = this.assets.open("hdr/castle-hdr.avif").source().buffer().readByteArray()
            val size = coder.getSize(buffer)!!
            assert(size != null)
            val bitmap = coder.decodeSampled(
                buffer,
                size.width * 2,
                size.height * 2,
                PreferredColorConfig.RGBA_F16,
                ScaleMode.RESIZE
            )
            binding.imageView.setImageBitmap(bitmap)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            val buffer = this.assets.open("hdr/future city.avif").source().buffer().readByteArray()
            val size = coder.getSize(buffer)!!
            assert(size != null)
            val bitmap = coder.decodeSampled(
                buffer,
                size.width * 2,
                size.height * 2,
                PreferredColorConfig.RGBA_8888,
                ScaleMode.RESIZE
            )
            binding.imageView1.setImageBitmap(bitmap)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            val buffer = this.assets.open("hdr/Sea of Umbrellas at Shibuya Crossing-hdr.avif").source().buffer().readByteArray()
            val size = coder.getSize(buffer)!!
            assert(size != null)
            val bitmap = coder.decodeSampled(
                buffer,
                size.width * 2,
                size.height * 2,
                PreferredColorConfig.RGBA_1010102,
                ScaleMode.RESIZE
            )
            binding.imageView2.setImageBitmap(bitmap)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            val buffer = this.assets.open("hdr/Elevate-hdr.avif").source().buffer().readByteArray()
            val size = coder.getSize(buffer)!!
            assert(size != null)
            val bitmap = coder.decodeSampled(
                buffer,
                size.width * 2,
                size.height * 2,
                PreferredColorConfig.RGBA_1010102,
                ScaleMode.RESIZE
            )
            binding.imageView3.setImageBitmap(bitmap)
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            val buffer = this.assets.open("hdr/house on lake.avif").source().buffer().readByteArray()
            val size = coder.getSize(buffer)!!
            assert(size != null)
            val bitmap = coder.decodeSampled(
                buffer,
                size.width * 2,
                size.height * 2,
                PreferredColorConfig.RGB_565,
                ScaleMode.RESIZE
            )
            binding.imageView4.setImageBitmap(bitmap)
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            val buffer = this.assets.open("hdr/Triad-hdr.avif").source().buffer().readByteArray()
            val size = coder.getSize(buffer)!!
            assert(size != null)
            val bitmap = coder.decodeSampled(
                buffer,
                size.width * 2,
                size.height * 2,
                PreferredColorConfig.RGBA_1010102,
                ScaleMode.RESIZE
            )
            binding.imageView5.setImageBitmap(bitmap)
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            val buffer = this.assets.open("hdr/GUM Mall Lights-hdr.avif").source().buffer().readByteArray()
            val size = coder.getSize(buffer)!!
            assert(size != null)
            val bitmap = coder.decodeSampled(
                buffer,
                size.width * 2,
                size.height * 2,
                PreferredColorConfig.RGBA_1010102,
                ScaleMode.RESIZE
            )
            binding.imageView6.setImageBitmap(bitmap)
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            val buffer = this.assets.open("bt_2020_pq.avif").source().buffer().readByteArray()
            val size = coder.getSize(buffer)!!
            assert(size != null)
            val bitmap = coder.decodeSampled(
                buffer,
                size.width / 3,
                size.height / 3,
                PreferredColorConfig.RGBA_1010102,
                ScaleMode.RESIZE
            )
            binding.imageView7.setImageBitmap(bitmap)
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            val buffer = this.assets.open("federico-beccari.avif").source().buffer().readByteArray()
            val size = coder.getSize(buffer)!!
            assert(size != null)
            val bitmap = coder.decodeSampled(
                buffer,
                size.width / 3,
                size.height / 3,
                PreferredColorConfig.RGBA_1010102,
                ScaleMode.RESIZE
            )
            binding.imageView8.setImageBitmap(bitmap)
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            val buffer = this.assets.open("federico-beccari-hlg.avif").source().buffer().readByteArray()
            val size = coder.getSize(buffer)!!
            assert(size != null)
            val bitmap = coder.decodeSampled(
                buffer,
                size.width / 2,
                size.height / 2,
                PreferredColorConfig.RGBA_1010102,
                ScaleMode.RESIZE
            )
            binding.imageView9.setImageBitmap(bitmap)
        }

        //https://wh.aimuse.online/creatives/IMUSE_03617fe2db82a584166_27/TT_a9d21ff1061d785347935fef/68f06252.avif
        //https://wh.aimuse.online/preset/federico-beccari.avif
        // https://wh.aimuse.online/preset/avif10bit.avif

//        Glide.with(this)
//            .load("https://wh.aimuse.online/preset/federico-beccari.avif")
//            .skipMemoryCache(true)
//            .into(binding.imageView)

//        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
//            binding.imageView.load("https://wh.aimuse.online/preset/federico-beccari.avif",
//                imageLoader = ImageLoader.Builder(this)
//                    .components {
//                        add(HeifDecoder.Factory())
//                    }
//                    .bitmapConfig(Bitmap.Config.HARDWARE)
//                    .build())
//        }

//        binding.imageView.setImageBitmap(bitmap)
//        binding.imageView.setImageBitmap(cc16)
//        val avif12DepthBuffer =
//            this.assets.open("test_avif_12_bitdepth.avif").source().buffer().readByteArray()
//        assert(HeifCoder().isAvif(avif12DepthBuffer))
//        val avifHDRBitmap = HeifCoder().decode(avif12DepthBuffer)
//        binding.imageView.setImageBitmap(avifHDRBitmap)
//        val heicBuffer = this.assets.open("pexels-heif.heif").source().buffer().readByteArray()
//        assert(HeifCoder().isHeif(heicBuffer))
//        val heicBitmap = HeifCoder().decode(heicBuffer)
//        binding.imageView.setImageBitmap(heicBitmap)
//        assert(HeifCoder().getSize(heicBuffer) != null)
//        assert(HeifCoder().getSize(buffer) != null)
//        val heicScaled = HeifCoder().decodeSampled(heicBuffer, 350, 900)
//        binding.imageView.setImageBitmap(heicScaled)
//        val extremlyLargeBitmapBuffer =
//            this.assets.open("extremly_large.avif").source().buffer().readByteArray()
//        assert(HeifCoder().isAvif(extremlyLargeBitmapBuffer))
//        val extremlyLargeBitmap = HeifCoder().decode(extremlyLargeBitmapBuffer)
//        binding.imageView.setImageBitmap(extremlyLargeBitmap)

//        val ff = File(this.filesDir, "result.avif")
//        ff.delete()
//        val output = FileOutputStream(ff)
//        output.sink().buffer().use {
//            it.write(bytes)
//            it.flush()
//        }
//        output.close()
//        Log.d("p", bytes.size.toString())
//        writeHevc(decodedBitmap)
//        val numbers = IntArray(5) { 1 * (it + 1) }
//        numbers.forEach {
//            testEncoder("test_${it}.jpg")
//        }
//        val bytes = HeifCoder().encodeAvif(cc16)
//        val ff = File(this.filesDir, "result.avif")
//        ff.delete()
//        val output = FileOutputStream(ff)
//        output.sink().buffer().use {
//            it.write(bytes)
//            it.flush()
//        }
//        output.close()
//        Log.d("p", bytes.size.toString())
//        writeHevc(decodedBitmap)
//        val numbers = IntArray(5) { 1 * (it + 1) }
//        numbers.forEach {
//            testEncoder("test_${it}.jpg")
//        }
    }

    private fun testEncoder(assetName: String) {
        val coder = HeifCoder(this)
        val buffer = this.assets.open(assetName).source().buffer().readByteArray()
        val opts = BitmapFactory.Options()
        opts.inMutable = true
        var bitmap = BitmapFactory.decodeByteArray(buffer, 0, buffer.size, opts)
        val rr = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            bitmap.copy(Bitmap.Config.RGBA_F16, true)
        } else {
            bitmap.copy(Bitmap.Config.ARGB_8888, true)
        }
        bitmap.recycle()
        bitmap = rr
//        val newBitmap = bitmap.aspectFit(bitmap.width, bitmap.height)
//        bitmap.recycle()
        val bytes = coder.encodeAvif(bitmap)
        bitmap.recycle()
        val ff = File(this.filesDir, "${File(assetName).nameWithoutExtension}.avif")
        ff.delete()
        val output = FileOutputStream(ff)
        output.sink().buffer().use {
            it.write(bytes)
            it.flush()
        }
        output.close()
        Log.d("p", bytes.size.toString())
    }

    fun Bitmap.aspectFit(maxWidth: Int, maxHeight: Int): Bitmap {
        val image = this
        val width: Int = image.width
        val height: Int = image.height
        val ratioBitmap = width.toFloat() / height.toFloat()
        val ratioMax = maxWidth.toFloat() / maxHeight.toFloat()

        var finalWidth = maxWidth
        var finalHeight = maxHeight
        if (ratioMax > ratioBitmap) {
            finalWidth = (maxHeight.toFloat() * ratioBitmap).toInt()
        } else {
            finalHeight = (maxWidth.toFloat() / ratioBitmap).toInt()
        }
        return Bitmap.createScaledBitmap(image, finalWidth, finalHeight, true)
    }

    fun aspectScale(sourceSize: Size, dstSize: Size): Size {
        val isHorizontal = sourceSize.width > sourceSize.height
        val targetSize = if (isHorizontal) dstSize else Size(dstSize.height, dstSize.width)
        if (targetSize.width > sourceSize.width && targetSize.width > sourceSize.height) {
            return sourceSize
        }
        val imageAspectRatio = sourceSize.width.toFloat() / sourceSize.height.toFloat()
        val canvasAspectRation = targetSize.width.toFloat() / targetSize.height.toFloat()
        val resizeFactor: Float
        if (imageAspectRatio > canvasAspectRation) {
            resizeFactor = targetSize.width.toFloat() / sourceSize.width.toFloat()
        } else {
            resizeFactor = targetSize.height.toFloat() / sourceSize.height.toFloat()
        }
        return Size(
            (sourceSize.width.toFloat() * resizeFactor).toInt(),
            (sourceSize.height.toFloat() * resizeFactor).toInt()
        )
    }

    private fun writeHevc(bitmap: Bitmap) {
        val bytes = HeifCoder(this).encodeHeic(bitmap)
        val ff = File(this.filesDir, "result.heic")
        ff.delete()
        val output = FileOutputStream(ff)
        output.sink().buffer().use {
            it.write(bytes)
            it.flush()
        }
        output.close()
        Log.d("p", bytes.size.toString())
    }

    /**
     * A native method that is implemented by the 'avif' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    companion object {
        // Used to load the 'avif' library on application startup.
        init {
            System.loadLibrary("avif")
        }
    }
}