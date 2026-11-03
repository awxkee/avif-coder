#pragma once

#include "ImageFrame.h"
#include "SizeScaler.h"
#include "avif/avif.h"

GainmapFrame DecodeGainmap(const avifGainMap &gain);

// Use the base pixel grid before applying its crop, including odd-sized crops.
void RescaleGainmap(GainmapFrame &gain, uint32_t baseWidth, uint32_t baseHeight,
                    int32_t scaledWidth, int32_t scaledHeight, ScaleMode mode,
                    int scalingQuality);

// Renders the ISO SDR rendition for systems that cannot attach this gainmap.
// The caller owns output's pixels and must call avifRGBImageFreePixels().
void RenderGainmapToSdr(const avifRGBImage &base, avifColorPrimaries primaries,
                        avifTransferCharacteristics transfer, const avifGainMap &gain,
                        avifRGBImage &output);
