package com.radzivon.bartoshyk.avif

import android.os.Build
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.radzivon.bartoshyk.avif.coder.Coder
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class GainmapCompatibilityInstrumentedTest {
    // Fixtures are generated solid 8x8 images with independent 4x4 gainmaps.
    private fun fixture(name: String) = InstrumentationRegistry.getInstrumentation()
        .context.assets.open("gainmaps/$name").use { it.readBytes() }

    @Test
    fun sdrBaseDecodesOnEverySupportedOs() {
        val bitmap = Coder().decode(fixture("sdr.avif"))
        assertEquals(8, bitmap.width)
        assertEquals(8, bitmap.height)
        if (Build.VERSION.SDK_INT >= 34) assertTrue(bitmap.hasGainmap())
    }

    @Test
    fun appleGainmapDecodesOnEverySupportedOs() {
        val bitmap = Coder().decode(fixture("apple.heic"))
        assertEquals(8, bitmap.width)
        assertEquals(8, bitmap.height)
        if (Build.VERSION.SDK_INT >= 34) assertTrue(bitmap.hasGainmap())
    }

    @Test
    fun hdrBaseRequiresAndroid16ForGainmapDirection() {
        val bitmap = Coder().decode(fixture("hdr.avif"))
        assertEquals(8, bitmap.width)
        assertEquals(8, bitmap.height)
        if (Build.VERSION.SDK_INT >= 34) {
            assertEquals(Build.VERSION.SDK_INT >= 36, bitmap.hasGainmap())
        }
    }
}
