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
import com.radzivon.bartoshyk.avif.coder.HeifCoder
import com.radzivon.bartoshyk.avif.databinding.ActivityMainBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
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

//        val buffer = this.assets.open("test_avif.avif").source().buffer().readByteArray()
//        assert(HeifCoder().isAvif(buffer))
//        val bitmap = HeifCoder().decode(buffer)
//        val opts = BitmapFactory.Options()
//        opts.inMutable = true
//        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
//            opts.inPreferredConfig = Bitmap.Config.RGBA_F16
//        }
        val decodedBitmap = BitmapFactory.decodeResource(resources, R.drawable.test_png_with_alpha)
        var cc16 = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            decodedBitmap.copy(Bitmap.Config.RGBA_F16, true)
        } else {
            decodedBitmap.copy(Bitmap.Config.ARGB_8888, true)
        }
        val rescaledSize =
            aspectScale(Size(decodedBitmap.width, decodedBitmap.height), Size(1400, 720))
        val cc16Rescaled = cc16.scale(rescaledSize.width, rescaledSize.height, true)
        cc16.recycle()
        cc16 = cc16Rescaled
        binding.imageView.setImageBitmap(cc16)
//        binding.imageView.setImageBitmap(decodedBitmap)
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

        val bytes = HeifCoder().encodeHeic(cc16)
        val decodedBytes = HeifCoder().decode(bytes)
        binding.imageView.setImageBitmap(decodedBytes)
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
        val bytes = HeifCoder().encodeAvif(bitmap)
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