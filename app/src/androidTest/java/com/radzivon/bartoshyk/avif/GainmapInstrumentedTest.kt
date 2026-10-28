package com.radzivon.bartoshyk.avif

import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.ColorSpace
import android.graphics.Gainmap
import android.os.Build
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.SdkSuppress
import com.radzivon.bartoshyk.avif.coder.AvSpeed
import com.radzivon.bartoshyk.avif.coder.AvKind
import com.radzivon.bartoshyk.avif.coder.AvifEncodingOptions
import com.radzivon.bartoshyk.avif.coder.Coder
import com.radzivon.bartoshyk.avif.coder.HeifContainerInnerType
import com.radzivon.bartoshyk.avif.coder.HevcEncodingOptions
import com.radzivon.bartoshyk.avif.coder.PreferredColorConfig
import com.radzivon.bartoshyk.avif.coder.ScaleMode
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.nio.ByteBuffer
import java.nio.ByteOrder

@RunWith(AndroidJUnit4::class)
@SdkSuppress(minSdkVersion = 34)
class GainmapInstrumentedTest {
    private fun source(rgb: Boolean = false): Bitmap {
        check(Build.VERSION.SDK_INT >= 34)
        val base = Bitmap.createBitmap(32, 24, Bitmap.Config.ARGB_8888)
        base.eraseColor(Color.rgb(90, 100, 110))
        val contents = Bitmap.createBitmap(16, 12, Bitmap.Config.ARGB_8888)
        contents.eraseColor(if (rgb) Color.rgb(64, 128, 192) else Color.rgb(128, 128, 128))
        base.gainmap = Gainmap(contents).apply {
            setRatioMax(4f, 4f, 4f)
            setGamma(0.75f, 1f, 1.25f)
            displayRatioForFullHdr = 4f
        }
        return base
    }

    private fun supportedEncoder() {
        assumeTrue(Build.VERSION.SDK_INT >= 34)
        assumeTrue(Build.SUPPORTED_ABIS.any { it == "arm64-v8a" || it == "armeabi-v7a" })
    }

    @Test
    fun avifPreservesRgbGainmapMetadataAndSampledDirectBufferDecoding() {
        supportedEncoder()
        val coder = Coder()
        val input = source(rgb = true)
        input.gainmap!!.setEpsilonSdr(0.01f, 0.02f, 0.03f)
        input.gainmap!!.setEpsilonHdr(0.02f, 0.03f, 0.04f)
        val encoded = coder.encodeAvif(input)
        val decoded = coder.decode(encoded)
        assertTrue(decoded.hasGainmap())
        val gainmap = decoded.gainmap!!
        assertArrayEquals(input.gainmap!!.ratioMax, gainmap.ratioMax, 0.001f)
        assertArrayEquals(input.gainmap!!.gamma, gainmap.gamma, 0.001f)
        assertArrayEquals(input.gainmap!!.epsilonSdr, gainmap.epsilonSdr, 0.0001f)
        assertArrayEquals(input.gainmap!!.epsilonHdr, gainmap.epsilonHdr, 0.0001f)
        val direct = ByteBuffer.allocateDirect(encoded.size).put(encoded).apply { rewind() }
        val sampled = coder.decodeSampled(direct, 16, 12)
        assertEquals(16, sampled.width)
        assertEquals(12, sampled.height)
        assertTrue(sampled.hasGainmap())
        val pixel = gainmap.gainmapContents.getPixel(4, 4)
        assertTrue(Color.red(pixel) < Color.green(pixel))
        assertTrue("Gainmap RGB: ${Color.red(pixel)}, ${Color.green(pixel)}, ${Color.blue(pixel)}", Color.green(pixel) < Color.blue(pixel))
    }

    @Test
    fun heicPreservesAppleCompatibleGainmap() {
        supportedEncoder()
        val input = source().apply { gainmap!!.setGamma(1f, 1f, 1f) }
        val coder = Coder()
        val output = coder.decode(coder.encodeHeic(input))
        assertTrue(output.hasGainmap())
        assertEquals(4f, output.gainmap!!.displayRatioForFullHdr, 0.001f)
        // Compare reconstructed gains, since Apple's sample curve differs from Android's.
        val sample = Color.red(output.gainmap!!.gainmapContents.getPixel(4, 4)) / 255f
        assertEquals(2f, Math.pow(4.0, sample.toDouble()).toFloat(), 0.12f)
        assertTrue(coder.decode(coder.encodeHeic(input,
            options = HevcEncodingOptions(ignoreGainmapErrors = false))).hasGainmap())
    }

    @Test
    fun extraSlowEncodesAndOrdinaryImagesDoNotAcquireGainmaps() {
        supportedEncoder()
        val input = Bitmap.createBitmap(8, 8, Bitmap.Config.ARGB_8888).apply { eraseColor(Color.RED) }
        val coder = Coder()
        val output = coder.decode(coder.encodeAvif(input, options = AvifEncodingOptions(speed = AvSpeed.EXTRA_SLOW)))
        assertFalse(output.hasGainmap())
    }

    @Test
    fun hardwareOutputKeepsGainmap() {
        supportedEncoder()
        val coder = Coder()
        val output = coder.decode(coder.encodeAvif(source()), PreferredColorConfig.HARDWARE)
        assertTrue(output.hasGainmap())
    }

    @Test
    fun highBitDepthGainmapStaysHighBitDepth() {
        supportedEncoder()
        val input = source()
        input.gainmap!!.gainmapContents = Bitmap.createBitmap(16, 12, Bitmap.Config.RGBA_F16).apply {
            eraseColor(Color.rgb(64, 128, 192))
        }
        val coder = Coder()
        val output = coder.decode(coder.encodeAvif(input))
        assertTrue(output.hasGainmap())
        assertEquals(Bitmap.Config.RGBA_F16, output.gainmap!!.gainmapContents.config)
    }

    @Test
    fun heicSampledFillCropsTheGainmapWithItsBase() {
        supportedEncoder()
        val input = source().apply { gainmap!!.setGamma(1f, 1f, 1f) }
        val map = input.gainmap!!.gainmapContents
        for (y in 0 until map.height) for (x in 0 until map.width) {
            val value = x * 255 / (map.width - 1)
            map.setPixel(x, y, Color.rgb(value, value, value))
        }
        val coder = Coder()
        val output = coder.decodeSampled(coder.encodeHeic(input), 8, 24, scaleMode = ScaleMode.FILL)
        val decodedMap = output.gainmap!!.gainmapContents
        assertEquals(8, decodedMap.width)
        assertEquals(24, decodedMap.height)
        assertTrue(Color.red(decodedMap.getPixel(0, 12)) > 60)
        assertTrue(Color.red(decodedMap.getPixel(7, 12)) < 195)
    }

    @Test
    fun heicRejectsGainmapsItCannotPreserve() {
        supportedEncoder()
        try {
            Coder().encodeHeic(source(rgb = true),
                options = HevcEncodingOptions(ignoreGainmapErrors = false))
            fail("RGB HEIC gainmap must not be silently converted to grayscale")
        } catch (error: RuntimeException) {
            assertTrue(error.message.orEmpty().contains("single gain channel"))
        }
    }

    @Test
    fun heicIgnoresUnsupportedGainmapsByDefault() {
        supportedEncoder()
        val coder = Coder()
        val rgb = source(rgb = true)
        assertFalse(coder.decode(coder.encodeHeic(rgb)).hasGainmap())
        assertNotNull(rgb.gainmap)
        val offsets = source().apply {
            gainmap!!.setGamma(1f, 1f, 1f)
            gainmap!!.setEpsilonSdr(0.01f, 0.01f, 0.01f)
            gainmap!!.setEpsilonHdr(0.01f, 0.01f, 0.01f)
        }
        val decoded = coder.decode(coder.encodeHeic(offsets))
        assertEquals(offsets.width, decoded.width)
        assertEquals(offsets.height, decoded.height)
        assertFalse(decoded.hasGainmap())
        try {
            coder.encodeHeic(offsets, options = HevcEncodingOptions(ignoreGainmapErrors = false))
            fail("Strict HEIC encoding must report nonzero gainmap offsets")
        } catch (error: RuntimeException) {
            assertTrue(error.message.orEmpty().contains("nonzero offsets"))
        }
    }

    @Test
    fun invalidGainmapSamplesCanBeIgnoredOrReported() {
        supportedEncoder()
        val input = source()
        val invalid = Bitmap.createBitmap(1, 1, Bitmap.Config.RGBA_F16)
        val pixels = ByteBuffer.allocate(8).order(ByteOrder.nativeOrder()).apply {
            putShort(0x4000.toShort()) // Red gain sample 2.0, outside the normalized range.
            putShort(0x3800.toShort())
            putShort(0x3800.toShort())
            putShort(0x3c00.toShort())
            rewind()
        }
        invalid.copyPixelsFromBuffer(pixels)
        input.gainmap!!.gainmapContents = invalid
        val coder = Coder()
        assertFalse(coder.decode(coder.encodeAvif(input)).hasGainmap())
        assertFalse(coder.decode(coder.encodeHeic(input)).hasGainmap())
        try {
            coder.encodeAvif(input, options = AvifEncodingOptions(ignoreGainmapErrors = false))
            fail("Strict AVIF encoding must report invalid gainmap samples")
        } catch (error: RuntimeException) {
            assertTrue(error.message.orEmpty().contains("finite and normalized"))
        }
    }

    @Test
    fun gainmapCodecFailureRetriesWithOriginalRgb565Pixels() {
        supportedEncoder()
        val input = Bitmap.createBitmap(32, 24, Bitmap.Config.RGB_565).apply {
            eraseColor(Color.rgb(90, 130, 170))
            // MaroonTree accepts at most 16383 pixels on either axis.
            gainmap = Gainmap(Bitmap.createBitmap(16384, 1, Bitmap.Config.ARGB_8888).apply {
                eraseColor(Color.GRAY)
            })
        }
        val coder = Coder()
        val decoded = coder.decode(coder.encodeAvif(input))
        assertEquals(input.width, decoded.width)
        assertFalse(decoded.hasGainmap())
        val expected = input.getPixel(16, 12)
        val actual = decoded.getPixel(16, 12)
        assertEquals(Color.red(expected).toFloat(), Color.red(actual).toFloat(), 12f)
        assertEquals(Color.green(expected).toFloat(), Color.green(actual).toFloat(), 12f)
        assertEquals(Color.blue(expected).toFloat(), Color.blue(actual).toFloat(), 12f)
        try {
            coder.encodeAvif(input, options = AvifEncodingOptions(ignoreGainmapErrors = false))
            fail("Strict AVIF encoding must report an unencodable gainmap")
        } catch (error: RuntimeException) {
            assertTrue(error.message.orEmpty().contains("Invalid image dimensions"))
        }
    }

    @Test
    fun lenientGainmapHandlingDoesNotHideBaseEncodingErrors() {
        supportedEncoder()
        val input = Bitmap.createBitmap(16384, 1, Bitmap.Config.ARGB_8888).apply {
            eraseColor(Color.GRAY)
            gainmap = source().gainmap
        }
        for (ignore in listOf(true, false)) {
            try {
                Coder().encodeAvif(input, options = AvifEncodingOptions(ignoreGainmapErrors = ignore))
                fail("Base encoding errors must propagate in both modes")
            } catch (error: RuntimeException) {
                assertTrue(error.message.orEmpty().contains("Invalid image dimensions"))
            }
        }
    }

    @Test
    fun av2IgnoresGainmapsByDefaultAndReportsThemInStrictMode() {
        supportedEncoder()
        val input = source(rgb = true)
        val coder = Coder()
        val options = AvifEncodingOptions(avKind = AvKind.AV2)
        val encoded = coder.encodeAvif(input, options = options)
        val buffer = ByteBuffer.allocateDirect(encoded.size).put(encoded).apply { rewind() }
        assertEquals(HeifContainerInnerType.AV2, coder.detectContainer(buffer))
        try {
            coder.encodeAvif(input, options = options.copy(ignoreGainmapErrors = false))
            fail("Strict AV2 encoding must report unsupported gainmaps")
        } catch (error: RuntimeException) {
            assertTrue(error.message.orEmpty().contains("AV2 gainmap encoding"))
        }
        input.gainmap = null
        assertTrue(coder.encodeAvif(input, options = options.copy(ignoreGainmapErrors = false)).isNotEmpty())
    }

    @Test
    @SdkSuppress(minSdkVersion = 36)
    fun hdrDirectionAndAlternativePrimariesRoundTripOnAndroid16() {
        supportedEncoder()
        assumeTrue(Build.VERSION.SDK_INT >= 36)
        val input = source(rgb = true)
        input.gainmap!!.gainmapDirection = Gainmap.GAINMAP_DIRECTION_HDR_TO_SDR
        input.gainmap!!.alternativeImagePrimaries = ColorSpace.get(ColorSpace.Named.DISPLAY_P3)
        val coder = Coder()
        val output = coder.decode(coder.encodeAvif(input))
        assertTrue(output.hasGainmap())
        assertEquals(Gainmap.GAINMAP_DIRECTION_HDR_TO_SDR, output.gainmap!!.gainmapDirection)
        assertEquals(ColorSpace.Named.DISPLAY_P3.ordinal, output.gainmap!!.alternativeImagePrimaries!!.id)
    }
}
