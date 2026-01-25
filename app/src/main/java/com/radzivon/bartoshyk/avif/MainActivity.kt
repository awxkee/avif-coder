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

import android.os.Bundle
import android.util.Log
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.radzivon.bartoshyk.avif.coder.HeifCoder
import com.radzivon.bartoshyk.avif.coder.PreferredColorConfig
import com.radzivon.bartoshyk.avif.coder.ScaleMode
import com.radzivon.bartoshyk.avif.databinding.ActivityMainBinding
import com.radzivon.bartoshyk.avif.databinding.BindingImageViewBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import okio.buffer
import okio.source
import java.io.FileNotFoundException
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
            val allFiles = mutableListOf<String>()
            allFiles.addAll(allFiles2)
            allFiles.addAll(allFiles1)
            // Removed filter - process all AVIF/HEIC/HEIF images
            // allFiles = allFiles.filter { it.contains("test_img444.avif") }.toMutableList()

            (0 until ITERATION_COUNT).forEach { _ ->
                for (file in allFiles) {
                    try {
                        Log.d("AVIF", "start processing $file")
                        val buffer = this@MainActivity.assets.open(file).source().buffer()
                            .readByteArray()

                        val size = coder.getSize(buffer)
                        if (size != null) {
                            Log.d("AVIF", "Image size: ${size.width}x${size.height}")
                            val start = System.currentTimeMillis()
                            val bitmap0 = coder.decodeSampled(
                                buffer,
                                if (size.width > LARGE_IMAGE_THRESHOLD || size.height > LARGE_IMAGE_THRESHOLD) {
                                    size.width / LARGE_IMAGE_SCALE_FACTOR
                                } else {
                                    size.width
                                },
                                if (size.width > LARGE_IMAGE_THRESHOLD || size.height > LARGE_IMAGE_THRESHOLD) {
                                    size.height / LARGE_IMAGE_SCALE_FACTOR
                                } else {
                                    size.height
                                },
                                PreferredColorConfig.RGBA_8888,
                                ScaleMode.RESIZE
                            )
                            val decodeTime = System.currentTimeMillis() - start
                            Log.d("AVIF", "Decode time: ${decodeTime}ms for $file")

                            lifecycleScope.launch(Dispatchers.Main) {
                                val imageView = BindingImageViewBinding.inflate(
                                    layoutInflater,
                                    binding.scrollViewContainer,
                                    false
                                )
                                imageView.root.setImageBitmap(bitmap0)
                                binding.scrollViewContainer.addView(imageView.root)
                                Log.d("AVIF", "Successfully displayed image: $file")
                            }
                        } else {
                            Log.w("AVIF", "getSize() returned null for file: $file")
                        }
                    } catch (e: Exception) {
                        Log.e("AVIF", "Error processing $file: ${e.message}", e)
                        if (e !is FileNotFoundException) {
                            throw e
                        }
                    }
                }
            }
        }
    }

    companion object {
        private const val ITERATION_COUNT = 5
        private const val LARGE_IMAGE_THRESHOLD = 1800
        private const val LARGE_IMAGE_SCALE_FACTOR = 4
        // Used to load the 'avif' library on application startup.
        init {
            System.loadLibrary("avif")
        }
    }
}