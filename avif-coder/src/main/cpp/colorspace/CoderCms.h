/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 14/01/2024
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

#if defined(HIGHWAY_HWY_CODER_CMS_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_CODER_CMS_INL_H_
#undef HIGHWAY_HWY_CODER_CMS_INL_H_
#else
#define HIGHWAY_HWY_CODER_CMS_INL_H_
#endif

#include "hwy/highway.h"

#import <vector>
#import "NEMath.h"
#include "CmsMath.hpp"

// https://64.github.io/tonemapping/
// https://www.russellcottrell.com/photo/matrixCalculator.htm

using namespace std;

#if defined(__clang__)
#pragma clang fp contract(fast) exceptions(ignore) reassociate(on)
#endif

#define HWY_CODER_CMS_INLINE inline __attribute__((flatten))

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

    using hwy::EnableIf;
    using hwy::IsFloat;
    using hwy::HWY_NAMESPACE::Vec;
    using hwy::HWY_NAMESPACE::Mul;
    using hwy::HWY_NAMESPACE::MulAdd;
    using hwy::HWY_NAMESPACE::LoadU;
    using hwy::HWY_NAMESPACE::Set;
    using hwy::HWY_NAMESPACE::TFromD;

    class ColorSpaceMatrix {
    public:
        ColorSpaceMatrix(const vector<vector<float>> primariesXy, const vector<float> whitePoint) {
            vector<vector<float>> mt = GamutRgbToXYZ(primariesXy, whitePoint);
            matrix.resize(9);
            SetVector(mt);
        }

        ColorSpaceMatrix(const float primariesXy[3][2], const float whitePoint[2]) {
            vector<vector<float>> mt = GamutRgbToXYZ(primariesXy, whitePoint);
            matrix.resize(9);
            SetVector(mt);
        }

        ColorSpaceMatrix(const vector<vector<float>> source) {
            matrix.resize(9);
            SetVector(source);
        }

        ~ColorSpaceMatrix() {

        }

#if __arm64__

        HWY_CODER_CMS_INLINE float32x4_t transfer(const float32x4_t v) {
        const float32x4_t row1 = { matrix[0], matrix[1], matrix[2], 0.0f };
        const float32x4_t row2 = { matrix[3], matrix[4], matrix[5], 0.0f };
        const float32x4_t row3 = { matrix[6], matrix[7], matrix[8], 0.0f };

        float32x4_t r = { vaddvq_f32(vmulq_f32(v, row1)), vaddvq_f32(vmulq_f32(v, row2)), vaddvq_f32(vmulq_f32(v, row3)), 0.0f };
        return r;
    }

    HWY_CODER_CMS_INLINE float32x4_t operator*(const float32x4_t v) {
        const float32x4_t row1 = { matrix[0], matrix[1], matrix[2], 0.0f };
        const float32x4_t row2 = { matrix[3], matrix[4], matrix[5], 0.0f };
        const float32x4_t row3 = { matrix[6], matrix[7], matrix[8], 0.0f };

        float32x4_t r = { vaddvq_f32(vmulq_f32(v, row1)), vaddvq_f32(vmulq_f32(v, row2)), vaddvq_f32(vmulq_f32(v, row3)), 0.0f };
        return r;
    }

    HWY_CODER_CMS_INLINE float32x4x4_t operator*(const float32x4x4_t v) {
        const float32x4_t r1 = { vaddvq_f32(vmulq_f32(v.val[0], row1)), vaddvq_f32(vmulq_f32(v.val[0], row2)), vaddvq_f32(vmulq_f32(v.val[0], row3)), 0.0f };
        const float32x4_t r2 = { vaddvq_f32(vmulq_f32(v.val[1], row1)), vaddvq_f32(vmulq_f32(v.val[1], row2)), vaddvq_f32(vmulq_f32(v.val[1], row3)), 0.0f };
        const float32x4_t r3= { vaddvq_f32(vmulq_f32(v.val[2], row1)), vaddvq_f32(vmulq_f32(v.val[2], row2)), vaddvq_f32(vmulq_f32(v.val[2], row3)), 0.0f };
        const float32x4_t r4 = { vaddvq_f32(vmulq_f32(v.val[3], row1)), vaddvq_f32(vmulq_f32(v.val[3], row2)), vaddvq_f32(vmulq_f32(v.val[3], row3)), 0.0f };
        float32x4x4_t r = { r1, r2, r3, r4 };
        return r;
    }

#endif

        HWY_CODER_CMS_INLINE ColorSpaceMatrix operator*(const ColorSpaceMatrix &other) {
            vector<vector<float>> resultMatrix(3, vector<float>(3, 0.0f));

            for (size_t i = 0; i < 3; ++i) {
                for (size_t j = 0; j < 3; ++j) {
                    for (size_t k = 0; k < 3; ++k) {
                        resultMatrix[i][j] += matrix[i * 3 + k] * other.matrix[k * 3 + j];
                    }
                }
            }

            return ColorSpaceMatrix(resultMatrix);
        }

        template<class D, typename V = Vec<D>, HWY_IF_FLOAT(TFromD<D>)>
        HWY_CODER_CMS_INLINE void convert(const D df, V &r, V &g, V &b) {
            const auto tLane0 = Set(df, static_cast<TFromD<D>>(matrix[0]));
            const auto tLane1 = Set(df, static_cast<TFromD<D>>(matrix[1]));
            const auto tLane2 = Set(df, static_cast<TFromD<D>>(matrix[2]));
            auto newR = MulAdd(r, tLane0, MulAdd(g, tLane1, Mul(b, tLane2)));
            const auto tLane3 = Set(df, static_cast<TFromD<D>>(matrix[3]));
            const auto tLane4 = Set(df, static_cast<TFromD<D>>(matrix[4]));
            const auto tLane5 = Set(df, static_cast<TFromD<D>>(matrix[5]));
            auto newG = MulAdd(r, tLane3, MulAdd(g, tLane4, Mul(b, tLane5)));
            const auto tLane6 = Set(df, static_cast<TFromD<D>>(matrix[6]));
            const auto tLane7 = Set(df, static_cast<TFromD<D>>(matrix[7]));
            const auto tLane8 = Set(df, static_cast<TFromD<D>>(matrix[8]));
            auto newB = MulAdd(r, tLane6, MulAdd(g, tLane7, Mul(b, tLane8)));
            r = newR;
            g = newG;
            b = newB;
        }

        HWY_CODER_CMS_INLINE void convert(float &r, float &g, float &b) {
#if __arm64
            float32x4_t v = { r, g, b, 0.0f };
            r = vdot_f32(v, row1);
            g = vdot_f32(v, row2);
            b = vdot_f32(v, row3);
#else
            const float newR = matrix[0] * r + matrix[1] * g + matrix[2] * b;
            const float newG = matrix[3] * r + matrix[4] * g + matrix[5] * b;
            const float newB = matrix[6] * r + matrix[7] * g + matrix[8] * b;
            r = newR;
            g = newG;
            b = newB;
#endif
        }

        void inverse() {
            vector<vector<float>> ret(3, std::vector<float>(3, 0.0f));
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    ret[i][j] = matrix[i * 3 + j];
                }
            }
            ret = inverseVector(ret);
            SetVector(ret);
        }

        ColorSpaceMatrix inverted() {
            vector<vector<float>> ret(3, std::vector<float>(3, 0.0f));
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    ret[i][j] = matrix[i * 3 + j];
                }
            }
            ret = inverseVector(ret);
            return ColorSpaceMatrix(ret);
        }

        vector<float> XyToXYZ(const float x, const float y) {
            vector<float> ret(3, 0.0f);

            ret[0] = x / y;
            ret[1] = 1.0;
            ret[2] = (1.0 - x - y) / y;

            return ret;
        }

        const vector<float> getWhitePoint(const float whitePoint[2]) {
            return XyToXYZ(whitePoint[0], whitePoint[1]);
        }

        const vector<float> getWhitePoint(const vector<float> whitePoint) {
            return XyToXYZ(whitePoint[0], whitePoint[1]);
        }

        vector<vector<float>>
        GamutRgbToXYZ(const vector<vector<float>> primariesXy, const vector<float> whitePoint) {
            const vector<vector<float>> xyZMatrix = getPrimariesXYZ(primariesXy);
            const vector<float> whiteXyz = getWhitePoint(whitePoint);
            const vector<float> s = mul(inverseVector(xyZMatrix), whiteXyz);
            const vector<vector<float>> m = {mul(xyZMatrix[0], s), mul(xyZMatrix[1], s),
                                             mul(xyZMatrix[2], s)};
            return m;
        }

        vector<vector<float>>
        GamutRgbToXYZ(const float primariesXy[3][2], const float whitePoint[2]) {
            const vector<vector<float>> xyZMatrix = getPrimariesXYZ(primariesXy);
            const vector<float> whiteXyz = getWhitePoint(whitePoint);
            const vector<float> s = mul(inverseVector(xyZMatrix), whiteXyz);
            const vector<vector<float>> m = {mul(xyZMatrix[0], s), mul(xyZMatrix[1], s),
                                             mul(xyZMatrix[2], s)};
            return m;
        }

    private:
        std::vector<float> matrix;

#if __arm64__
        float32x4_t row1;
    float32x4_t row2;
    float32x4_t row3;
#endif

        void SetVector(const vector<vector<float>> m) {
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    matrix[i * 3 + j] = m[i][j];
                }
            }

#if __arm64
            row1 = { matrix[0], matrix[1], matrix[2], 0.0f };
        row2 = { matrix[3], matrix[4], matrix[5], 0.0f };
        row3 = { matrix[6], matrix[7], matrix[8], 0.0f };
#endif
        }

        const vector<vector<float>> inverseVector(const vector<vector<float>> m) {
            vector<vector<float>> ret(3, std::vector<float>(3, 0.0f));
            const float det = determinant(m);
            ret[0][0] = det2(m[1][1], m[1][2], m[2][1], m[2][2]) / det;
            ret[0][1] = det2(m[0][2], m[0][1], m[2][2], m[2][1]) / det;
            ret[0][2] = det2(m[0][1], m[0][2], m[1][1], m[1][2]) / det;
            ret[1][0] = det2(m[1][2], m[1][0], m[2][2], m[2][0]) / det;
            ret[1][1] = det2(m[0][0], m[0][2], m[2][0], m[2][2]) / det;
            ret[1][2] = det2(m[0][2], m[0][0], m[1][2], m[1][0]) / det;
            ret[2][0] = det2(m[1][0], m[1][1], m[2][0], m[2][1]) / det;
            ret[2][1] = det2(m[0][1], m[0][0], m[2][1], m[2][0]) / det;
            ret[2][2] = det2(m[0][0], m[0][1], m[1][0], m[1][1]) / det;
            return ret;
        }

        const vector<vector<float>> getPrimariesXYZ(const float primaries_xy[3][2]) {
            // Columns: R G B
            // Rows: X Y Z
            vector<vector<float>> ret(3, std::vector<float>(3, 0.0f));

            ret[0] = XyToXYZ(primaries_xy[0][0], primaries_xy[0][1]);
            ret[1] = XyToXYZ(primaries_xy[1][0], primaries_xy[1][1]);
            ret[2] = XyToXYZ(primaries_xy[2][0], primaries_xy[2][1]);

            return transpose(ret);
        }

        const vector<vector<float>> getPrimariesXYZ(const vector<vector<float>> primaries_xy) {
            // Columns: R G B
            // Rows: X Y Z
            vector<vector<float>> ret(3, std::vector<float>(3, 0.0f));

            ret[0] = XyToXYZ(primaries_xy[0][0], primaries_xy[0][1]);
            ret[1] = XyToXYZ(primaries_xy[1][0], primaries_xy[1][1]);
            ret[2] = XyToXYZ(primaries_xy[2][0], primaries_xy[2][1]);

            return transpose(ret);
        }

        const float det2(const float a00, const float a01, const float a10, const float a11) {
            return a00 * a11 - a01 * a10;
        }

        const float determinant(const vector<vector<float>> m) {
            float det = 0;

            det += m[0][0] * det2(m[1][1], m[1][2], m[2][1], m[2][2]);
            det -= m[0][1] * det2(m[1][0], m[1][2], m[2][0], m[2][2]);
            det += m[0][2] * det2(m[1][0], m[1][1], m[2][0], m[2][1]);

            return det;
        }

        std::vector<std::vector<float>> transpose(const std::vector<std::vector<float>> &matrix) {
            if (matrix.size() != 3 || matrix[0].size() != 3) {
                throw std::invalid_argument("Input matrix must be 3x3");
            }

            std::vector<std::vector<float>> result(3, std::vector<float>(3, 0.0f));

            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    result[i][j] = matrix[j][i];
                }
            }

            return result;
        }

    };
}

HWY_AFTER_NAMESPACE();

#endif /* Colorspace_h */
