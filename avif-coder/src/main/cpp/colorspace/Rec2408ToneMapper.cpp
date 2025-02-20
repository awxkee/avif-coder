/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 13/10/2024
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

#include "Rec2408ToneMapper.h"
#include "Trc.h"
#include "Oklab.hpp"

float rec2408_pq(float intensity, const float intensity_target) {
  // Lb, Lw, Lmin, Lmax
  float luminances[4] = {
      10000.f / intensity_target,
      0.f / intensity_target,
      250.f / intensity_target,
      0.f / intensity_target,
  };

  for (float &luminance : luminances) {
    luminance = avifToGammaPQ(luminance);
  }

  // Step 1
  float source_pq_diff = luminances[1] - luminances[0];
  float normalized_source_pq_sample = (intensity - luminances[0]) / source_pq_diff;
  float min_luminance = (luminances[2] - luminances[0]) / source_pq_diff;
  float max_luminance = (luminances[3] - luminances[0]) / source_pq_diff;

  // Step 2
  float ks = 1.5f * max_luminance - 0.5f;
  float b = min_luminance;

  float compressed_pq_sample;

  // Step 3
  if (normalized_source_pq_sample < ks) {
    compressed_pq_sample = normalized_source_pq_sample;
  } else {
    // Step 4
    float one_sub_ks = 1.0f - ks;
    float t = (normalized_source_pq_sample - ks) / one_sub_ks;
    float t_p2 = t * t;
    float t_p3 = t_p2 * t;
    compressed_pq_sample = (2.0f * t_p3 - 3.0f * t_p2 + 1.0f) * ks
        + (t_p3 - 2.0f * t_p2 + t) * one_sub_ks
        + (-2.0f * t_p3 + 3.0f * t_p2) * max_luminance;
  }

  float lk = 1.f - compressed_pq_sample;

  float one_sub_compressed_p4 = lk * lk * lk * lk;
  float normalized_target_pq_sample = one_sub_compressed_p4 * b + compressed_pq_sample;
//
//  // Step 5
  return normalized_target_pq_sample * source_pq_diff + luminances[0];
}

void Rec2408ToneMapper::transferTone(float *inPlace, uint32_t width) {
  float *targetPlace = inPlace;

  const float vWeightA = this->weightA;
  const float vWeightB = this->weightB;

  for (uint32_t x = 0; x < width; ++x) {
    float r = targetPlace[0];
    float g = targetPlace[1];
    float b = targetPlace[2];
    float inLight = 0.2627f * static_cast<float>(r) + 0.6780f * static_cast<float>(g)
        + 0.0593f * static_cast<float>(b);
    if (inLight == 0) {
      continue;
    }
    float scale = (1.f + vWeightA * inLight) / (1.f + vWeightB * inLight);
    targetPlace[0] = std::min(r * scale, 1.f);
    targetPlace[1] = std::min(g * scale, 1.f);
    targetPlace[2] = std::min(b * scale, 1.f);
    targetPlace += 3;
  }
}