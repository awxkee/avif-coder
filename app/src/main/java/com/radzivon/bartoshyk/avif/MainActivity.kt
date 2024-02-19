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
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.util.Size
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.radzivon.bartoshyk.avif.coder.HeifCoder
import com.radzivon.bartoshyk.avif.coder.PreferredColorConfig
import com.radzivon.bartoshyk.avif.coder.ScaleMode
import com.radzivon.bartoshyk.avif.coder.ToneMapper
import com.radzivon.bartoshyk.avif.databinding.ActivityMainBinding
import com.radzivon.bartoshyk.avif.databinding.BindingImageViewBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import okio.FileNotFoundException
import okio.buffer
import okio.sink
import okio.source
import java.io.File
import java.io.FileOutputStream
import java.io.IOException


class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    fun getAllFilesFromAssets(path: String = ""): List<String> {
        val assetManager = assets
        val fileList: MutableList<String> = mutableListOf()

        try {
            // List all files in the "assets" folder
            val files = assetManager.list(path) ?: arrayOf()
            // Add each file to the list
            for (file in files) {
                if (path.isEmpty()) {
                    fileList.add(file)
                } else {
                    fileList.add("${path}/${file}")
                }
            }
        } catch (e: IOException) {
            e.printStackTrace()
        }

        return fileList
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Example of a call to a native method
//
        /*
        2023-11-05 20:23:49.798 24181-24181 AVIF                    com.radzivon.bartoshyk.avif          I  execution time 820
2023-11-05 20:23:51.574 24181-24181 AVIF                    com.radzivon.bartoshyk.avif          I  execution time 821
2023-11-05 20:23:52.341 24181-24181 AVIF                    com.radzivon.bartoshyk.avif          I  execution time 767
         */

        // HDR EXAMPLES - https://us.zonerama.com/williamskeaguidingphotography/Photo/1000120226/1004888131
        lifecycleScope.launch(Dispatchers.IO) {
            val coder = HeifCoder(this@MainActivity, toneMapper = ToneMapper.LOGARITHMIC)
            val allFiles1 = getAllFilesFromAssets().filter { it.contains(".avif") || it.contains(".heic") }
            val allFiles2 = getAllFilesFromAssets(path = "hdr").filter { it.contains(".avif") || it.contains(".heic") }
            var allFiles = mutableListOf<String>()
            allFiles.addAll(allFiles2)
            allFiles.addAll(allFiles1)
//            allFiles = allFiles.takeLast(4).toMutableList()
            for (file in allFiles) {
                try {
                    val buffer = this@MainActivity.assets.open(file).source().buffer()
                            .readByteArray()
                    val size = coder.getSize(buffer)
                    if (size != null) {
                        val bitmap = coder.decodeSampled(
                                buffer,
                                if (size.width > 1800 || size.height > 1800) size.width / 2 else size.width,
                                if (size.width > 1800 || size.height > 1800) size.height / 2 else size.height,
                                PreferredColorConfig.RGBA_8888,
                                ScaleMode.RESIZE
                        )
                        coder.encodeAvif(bitmap)
                        lifecycleScope.launch(Dispatchers.Main) {
                            val imageView = BindingImageViewBinding.inflate(layoutInflater, binding.scrollViewContainer, false)
                            imageView.root.setImageBitmap(bitmap)
                            binding.scrollViewContainer.addView(imageView.root)
                        }
                    }
                } catch (e: Exception) {
                    if (e is FileNotFoundException || e is java.io.FileNotFoundException) {

                    } else {
                        throw e
                    }
                }
            }
        }

//        https://wh.aimuse.online/creatives/IMUSE_03617fe2db82a584166_27/TT_a9d21ff1061d785347935fef/68f06252.avif
//        https://wh.aimuse.online/preset/federico-beccari.avif
//         https://wh.aimuse.online/preset/avif10bit.avif
//
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
//                    .bitmapConfig(Bitmap.Config.RGBA_F16)
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
            bitmap.copy(Bitmap.Config.ARGB_8888, true)
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

    companion object {
        // Used to load the 'avif' library on application startup.
        init {
            System.loadLibrary("avif")
        }
    }
}