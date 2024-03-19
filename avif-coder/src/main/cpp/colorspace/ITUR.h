/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * avif-coder [https://github.com/awxkee/avif-coder]
 *
 * Created by Radzivon Bartoshyk on 18/03/2024
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

#pragma once

#include "Eigen/Eigen"

struct ITURColorCoefficients {
  float kr;
  float kb;
  float kg;
};

static ITURColorCoefficients colorPrimariesComputeYCoeffs(Eigen::Matrix<float, 3, 2> colorPrimaries,
                                                          Eigen::Vector2f whitePoint) {
  float const rX = colorPrimaries(0, 0);
  float const rY = colorPrimaries(0, 1);
  float const gX = colorPrimaries(1, 0);
  float const gY = colorPrimaries(1, 1);
  float const bX = colorPrimaries(2, 0);
  float const bY = colorPrimaries(2, 1);
  float const wX = whitePoint(0);
  float const wY = whitePoint(1);
  float const rZ = 1.0f - (rX + rY); // (Eq. 34)
  float const gZ = 1.0f - (gX + gY); // (Eq. 35)
  float const bZ = 1.0f - (bX + bY); // (Eq. 36)
  float const wZ = 1.0f - (wX + wY); // (Eq. 37)
  float const
      kr = (rY * (wX * (gY * bZ - bY * gZ) + wY * (bX * gZ - gX * bZ) + wZ * (gX * bY - bX * gY))) /
      (wY * (rX * (gY * bZ - bY * gZ) + gX * (bY * rZ - rY * bZ) + bX * (rY * gZ - gY * rZ)));
  // (Eq. 32)
  float const
      kb = (bY * (wX * (rY * gZ - gY * rZ) + wY * (gX * rZ - rX * gZ) + wZ * (rX * gY - gX * rY))) /
      (wY * (rX * (gY * bZ - bY * gZ) + gX * (bY * rZ - rY * bZ) + bX * (rY * gZ - gY * rZ)));
  // (Eq. 33)

  return {
      .kr = kr,
      .kb = kb,
      .kg = 1.0f - kr - kb
  };
}
