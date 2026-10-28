package com.radzivon.bartoshyk.avif.coder

import android.graphics.Bitmap
import android.graphics.ColorSpace
import android.graphics.Gainmap
import android.os.Build
import androidx.annotation.Keep
import kotlin.math.log2
import kotlin.math.pow

/** JNI bridge. Metadata uses ISO 21496-1 log2 gains and encoding gamma. */
@Keep
internal object GainmapSupport {
    @JvmStatic
    fun contents(bitmap: Bitmap): Bitmap? {
        if (Build.VERSION.SDK_INT < 34) return null
        val contents = bitmap.gainmap?.gainmapContents ?: return null
        return if (contents.config == Bitmap.Config.HARDWARE) {
            contents.copy(Bitmap.Config.ARGB_8888, false)
                ?: error("Unable to copy hardware gainmap")
        } else contents
    }

    @JvmStatic
    fun metadata(bitmap: Bitmap): FloatArray? {
        if (Build.VERSION.SDK_INT < 34) return null
        val gainmap = bitmap.gainmap ?: return null
        val hdrBase = Build.VERSION.SDK_INT >= 36 &&
            gainmap.gainmapDirection == Gainmap.GAINMAP_DIRECTION_HDR_TO_SDR
        val alternate = if (Build.VERSION.SDK_INT >= 36) {
            gainmap.alternativeImagePrimaries
        } else null
        return FloatArray(19).apply {
            gainmap.ratioMin.forEachIndexed { c, v -> this[c] = log2(v) }
            gainmap.ratioMax.forEachIndexed { c, v -> this[3 + c] = log2(v) }
            gainmap.gamma.forEachIndexed { c, v -> this[6 + c] = 1f / v }
            (if (hdrBase) gainmap.epsilonHdr else gainmap.epsilonSdr).copyInto(this, 9)
            (if (hdrBase) gainmap.epsilonSdr else gainmap.epsilonHdr).copyInto(this, 12)
            val low = log2(gainmap.minDisplayRatioForHdrTransition)
            val high = log2(gainmap.displayRatioForFullHdr)
            this[15] = if (hdrBase) high else low
            this[16] = if (hdrBase) low else high
            this[17] = if (alternate == null) 1f else 0f
            this[18] = (alternate?.dataSpace ?: -1).toFloat()
            require(alternate == null || alternate.dataSpace != 0) {
                "Gainmap encoding requires named alternative image primaries"
            }
        }
    }

    @JvmStatic
    fun attach(bitmap: Bitmap, contents: Bitmap, metadata: FloatArray) {
        if (Build.VERSION.SDK_INT < 34) return
        require(metadata.size == 19 && metadata.all { it.isFinite() }) {
            "Invalid gainmap metadata"
        }
        val hdrBase = metadata[15] > metadata[16]
        // Android 14/15 expose only SDR-base gainmaps.
        if (hdrBase && Build.VERSION.SDK_INT < 36) return
        val gainmap = Gainmap(contents)
        gainmap.setRatioMin(2f.pow(metadata[0]), 2f.pow(metadata[1]), 2f.pow(metadata[2]))
        gainmap.setRatioMax(2f.pow(metadata[3]), 2f.pow(metadata[4]), 2f.pow(metadata[5]))
        gainmap.setGamma(1f / metadata[6], 1f / metadata[7], 1f / metadata[8])
        val sdr = if (hdrBase) 12 else 9
        val hdr = if (hdrBase) 9 else 12
        gainmap.setEpsilonSdr(metadata[sdr], metadata[sdr + 1], metadata[sdr + 2])
        gainmap.setEpsilonHdr(metadata[hdr], metadata[hdr + 1], metadata[hdr + 2])
        gainmap.minDisplayRatioForHdrTransition = 2f.pow(minOf(metadata[15], metadata[16]))
        gainmap.displayRatioForFullHdr = 2f.pow(maxOf(metadata[15], metadata[16]))
        if (Build.VERSION.SDK_INT >= 36) {
            gainmap.gainmapDirection = if (hdrBase) Gainmap.GAINMAP_DIRECTION_HDR_TO_SDR
                else Gainmap.GAINMAP_DIRECTION_SDR_TO_HDR
            if (metadata[17] == 0f && metadata[18] >= 0f) {
                gainmap.alternativeImagePrimaries = ColorSpace.getFromDataSpace(metadata[18].toInt())
            }
        }
        bitmap.gainmap = gainmap
    }

    @JvmStatic
    fun setColorSpace(bitmap: Bitmap, primaries: Int, transfer: Int) {
        if (Build.VERSION.SDK_INT < 34) return
        val name = when {
            primaries == 9 && transfer == 16 -> ColorSpace.Named.BT2020_PQ
            primaries == 9 && transfer == 18 -> ColorSpace.Named.BT2020_HLG
            primaries == 9 -> ColorSpace.Named.BT2020
            primaries == 12 -> ColorSpace.Named.DISPLAY_P3
            primaries == 1 && transfer == 8 -> ColorSpace.Named.LINEAR_SRGB
            primaries == 1 && transfer == 1 -> ColorSpace.Named.BT709
            else -> ColorSpace.Named.SRGB
        }
        bitmap.setColorSpace(ColorSpace.get(name))
    }
}
