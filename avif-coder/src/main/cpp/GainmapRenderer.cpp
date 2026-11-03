#include "GainmapRenderer.h"
#include "avif/avif_cxx.h"
#include "avifweaver.h"
#include <stdexcept>

namespace {
void ResizeGainmap(GainmapFrame &gain, int32_t width, int32_t height,
                    ScaleMode mode, int quality) {
  uint32_t stride = gain.width * 4 * (gain.bitDepth > 8 ? 2 : 1);
  gain.store = RescaleSourceImage(gain.store.data(), &stride, gain.bitDepth,
                                  gain.bitDepth > 8, &gain.width, &gain.height,
                                  width, height, mode, quality, false);
}
}

GainmapFrame DecodeGainmap(const avifGainMap &gain) {
  auto fraction = [](auto value) -> float {
    if (!value.d) throw std::runtime_error("Invalid gainmap fraction");
    return static_cast<float>(value.n) / static_cast<float>(value.d);
  };
  GainmapFrame decoded{};
  for (int c = 0; c < 3; ++c) {
    decoded.metadata[c] = fraction(gain.gainMapMin[c]);
    decoded.metadata[3 + c] = fraction(gain.gainMapMax[c]);
    decoded.metadata[6 + c] = fraction(gain.gainMapGamma[c]);
    decoded.metadata[9 + c] = fraction(gain.baseOffset[c]);
    decoded.metadata[12 + c] = fraction(gain.alternateOffset[c]);
  }
  decoded.metadata[15] = fraction(gain.baseHdrHeadroom);
  decoded.metadata[16] = fraction(gain.alternateHdrHeadroom);
  decoded.metadata[17] = gain.useBaseColorSpace ? 1.f : 0.f;
  decoded.metadata[18] = -1.f;
  if (gain.altColorPrimaries == AVIF_COLOR_PRIMARIES_BT709)
    decoded.metadata[18] = 0x08810000;
  else if (gain.altColorPrimaries == AVIF_COLOR_PRIMARIES_SMPTE432)
    decoded.metadata[18] = 0x088A0000;
  else if (gain.altColorPrimaries == AVIF_COLOR_PRIMARIES_BT2020)
    decoded.metadata[18] = 0x08C60000;

  avifRGBImage rgb{};
  avifRGBImageSetDefaults(&rgb, gain.image);
  rgb.format = AVIF_RGB_FORMAT_RGBA;
  rgb.depth = gain.image->depth;
  // Gain samples are data. Keep opaque alpha and never premultiply them.
  rgb.ignoreAlpha = AVIF_FALSE;
  avif::RGBImageCleanup cleanup(&rgb);
  if (avifRGBImageAllocatePixels(&rgb) != AVIF_RESULT_OK)
    throw std::runtime_error("Cannot allocate gainmap pixels");
  if (avifImageYUVToRGB(gain.image, &rgb) != AVIF_RESULT_OK)
    throw std::runtime_error("Cannot decode gainmap pixels");
  decoded.width = rgb.width;
  decoded.height = rgb.height;
  decoded.bitDepth = rgb.depth;
  auto stride = rgb.rowBytes;
  decoded.store = RescaleSourceImage(rgb.pixels, &stride, rgb.depth, rgb.depth > 8,
                                     &decoded.width, &decoded.height, 0, 0,
                                     Resize, 0, false);
  return decoded;
}

void RescaleGainmap(GainmapFrame &gain, uint32_t baseWidth, uint32_t baseHeight,
                    int32_t scaledWidth, int32_t scaledHeight, ScaleMode mode,
                    int scalingQuality) {
  if (scaledWidth == 0 && scaledHeight == 0) return;
  // Resolve auto dimensions and FIT/FILL against the base, not the auxiliary
  // image. Independent rounding moves crop boundaries on odd-sized images.
  if (gain.width != baseWidth || gain.height != baseHeight)
    ResizeGainmap(gain, baseWidth, baseHeight, Resize, scalingQuality);
  ResizeGainmap(gain, scaledWidth, scaledHeight, mode, scalingQuality);
}

void RenderGainmapToSdr(const avifRGBImage &base, avifColorPrimaries primaries,
                        avifTransferCharacteristics transfer, const avifGainMap &gain,
                        avifRGBImage &output) {
  auto decoded = DecodeGainmap(gain);
  output.format = AVIF_RGB_FORMAT_RGBA;
  output.depth = base.depth;
  output.width = base.width;
  output.height = base.height;
  if (avifRGBImageAllocatePixels(&output) != AVIF_RESULT_OK)
    throw std::runtime_error("Cannot allocate tone mapped pixels");
  const GainmapRenderImage basePixels{base.pixels, base.rowBytes, base.width, base.height, base.depth};
  const GainmapRenderImage gainPixels{decoded.store.data(),
                                      decoded.width * 4 * (decoded.bitDepth > 8 ? 2u : 1u),
                                      decoded.width, decoded.height, decoded.bitDepth};
  if (!weave_render_gainmap_sdr(basePixels, gainPixels, decoded.metadata.data(),
                                primaries, transfer, gain.altColorPrimaries,
                                output.pixels, output.rowBytes))
    throw std::runtime_error("Cannot tone map gainmap");
}
