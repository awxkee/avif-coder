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
import android.graphics.ColorSpace
import android.hardware.DataSpace
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.util.Size
import androidx.appcompat.app.AppCompatActivity
import androidx.core.graphics.scale
import androidx.lifecycle.lifecycleScope
import com.radzivon.bartoshyk.avif.coder.AvifChromaSubsampling
import com.radzivon.bartoshyk.avif.coder.HeifCoder
import com.radzivon.bartoshyk.avif.coder.PreferredColorConfig
import com.radzivon.bartoshyk.avif.coder.ScaleMode
import com.radzivon.bartoshyk.avif.coder.ToneMapper
import com.radzivon.bartoshyk.avif.databinding.ActivityMainBinding
import com.radzivon.bartoshyk.avif.databinding.BindingImageViewBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import okio.buffer
import okio.sink
import okio.source
import java.io.ByteArrayInputStream
import java.io.File
import java.io.FileNotFoundException
import java.io.FileOutputStream
import java.io.IOException
import kotlin.system.measureTimeMillis


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
//            lifecycleScope.launch(Dispatchers.Main) {
//                val imageView = BindingImageViewBinding.inflate(layoutInflater, binding.scrollViewContainer, false)
//                imageView.root.setImageBitmap(decoded100)
//                binding.scrollViewContainer.addView(imageView.root)
//            }
            val coder = HeifCoder()
            val allFiles1 =
                getAllFilesFromAssets().filter {
                    it.contains(".avif") || it.contains(".heic") || it.contains(
                        ".heif"
                    )
                }
            val allFiles2 =
                getAllFilesFromAssets(path = "hdr").filter {
                    it.contains(".avif") || it.contains(".heic") || it.contains(
                        ".heif"
                    )
                }
            var allFiles = mutableListOf<String>()
            allFiles.addAll(allFiles2)
            allFiles.addAll(allFiles1)
//            allFiles = allFiles.take(5).toMutableList()
//            allFiles = allFiles.filter { it.contains("hato-wide-gamut-8bit.avif") || it.contains("wide_gamut.avif") || it.contains("IMG_0199_rr.avif") || it.contains("bt_2020_pq.avif") }.toMutableList()
//            allFiles = allFiles.filter { it.contains("bbb_alpha_inverted.avif") }.toMutableList()
//            allFiles = allFiles.filter { it.contains("federico-beccari.avif") }.toMutableList()
            for (file in allFiles) {
                try {
                    Log.d("AVIF", "start processing $file")
                    val buffer = this@MainActivity.assets.open(file).source().buffer()
                        .readByteArray()

                    val size = coder.getSize(buffer)
                    if (size != null) {
                        val bitmap0 = coder.decodeSampled(
                            buffer,
                            if (size.width > 1800 || size.height > 1800) size.width / 4 else size.width,
                            if (size.width > 1800 || size.height > 1800) size.height / 4 else size.height,
                            PreferredColorConfig.RGBA_8888,
                            ScaleMode.RESIZE
                        )
//                        val bitmap0 = coder.decode(
//                            buffer,
//                        )
                        var start = System.currentTimeMillis()

//                        var bitmap0 = coder.decode(
//                            byteArray = buffer,
//                            preferredColorConfig = PreferredColorConfig.RGBA_F16,
//                        )

                        Log.d("AVIFFFF", "Decode time ${System.currentTimeMillis() - start}")

//                        val encode = coder.encodeAvif(bitmap0, avifChromaSubsampling = AvifChromaSubsampling.YUV420)
//                        val roundTripped = coder.decode(encode)
//
//
//                        val round = coder.decode(
//                            byteArray = encode,
//                            preferredColorConfig = PreferredColorConfig.RGBA_8888,
//                        )

//                        bitmap0.setColorSpace(ColorSpace.getFromDataSpace(DataSpace.DATASPACE_BT2020_PQ)!!)

                        lifecycleScope.launch(Dispatchers.Main) {
                            val imageView = BindingImageViewBinding.inflate(
                                layoutInflater,
                                binding.scrollViewContainer,
                                false
                            )
                            imageView.root.setImageBitmap(bitmap0)
                            binding.scrollViewContainer.addView(imageView.root)
                        }
//                        lifecycleScope.launch(Dispatchers.Main) {
//                            val imageView = BindingImageViewBinding.inflate(
//                                layoutInflater,
//                                binding.scrollViewContainer,
//                                false
//                            )
//                            imageView.root.setImageBitmap(round)
//                            binding.scrollViewContainer.addView(imageView.root)
//                        }
                    }
                } catch (e: Exception) {
                    Log.d("AVIF", e.toString())
                    if (e is FileNotFoundException || e is java.io.FileNotFoundException) {
                    } else {
                        throw e
                    }
                }
            }
        }
    }

    private fun testEncoder(assetName: String) {
        val coder = HeifCoder()
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
        val bytes = HeifCoder().encodeHeic(bitmap)
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