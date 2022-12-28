package com.radzivon.bartoshyk.avif

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import com.radzivon.bartoshyk.avif.coder.HeifCoder
import com.radzivon.bartoshyk.avif.databinding.ActivityMainBinding
import okio.buffer
import okio.sink
import okio.source
import java.io.File
import java.io.FileOutputStream

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Example of a call to a native method
        binding.sampleText.text = HeifCoder().stringFromJNI()

        val buffer = this.assets.open("test_avif.avif").source().buffer().readByteArray()
        val bitmap = HeifCoder().decodeAvif(buffer)
        val decodedBitmap = BitmapFactory.decodeResource(resources, R.drawable.test_png)
        binding.imageView.setImageBitmap(decodedBitmap)
        binding.imageView.setImageBitmap(bitmap)
        val bytes = HeifCoder().encodeAvif(decodedBitmap)
        val ff = File(this.filesDir, "result.avif")
        ff.delete()
        val output = FileOutputStream(ff)
        output.sink().buffer().use {
            it.write(bytes)
            it.flush()
        }
        output.close()
        Log.d("p", bytes.size.toString())
        writeHevc(decodedBitmap)
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