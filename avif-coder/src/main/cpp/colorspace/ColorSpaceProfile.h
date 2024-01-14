//
// Created by Radzivon Bartoshyk on 14/01/2024.
//

#ifndef JXLCODER_COLORSPACEPROFILE_H
#define JXLCODER_COLORSPACEPROFILE_H

#include <memory>
#include <vector>
#include "CmsMath.hpp"

using namespace std;

static const float Rec709Primaries[3][2] = {{0.640, 0.330},
                                            {0.300, 0.600},
                                            {0.150, 0.060}};
static const float Rec2020Primaries[3][2] = {{0.708, 0.292},
                                             {0.170, 0.797},
                                             {0.131, 0.046}};
static const float DisplayP3Primaries[3][2] = {{0.740, 0.270},
                                               {0.220, 0.780},
                                               {0.090, -0.090}};

static const float Rec2020LumaPrimaries[3] = {0.2627f, 0.6780f, 0.0593f};
static const float Rec709LumaPrimaries[3] = {0.2126f, 0.7152f, 0.0722f};
static const float DisplayP3LumaPrimaries[3] = {0.299f, 0.587f, 0.114f};
static const float DisplayP3WhitePointNits = 80;
static const float Rec709WhitePointNits = 100;
static const float Rec2020WhitePointNits = 203;

static const float IlluminantD65[2] = {0.3127, 0.3290};

class ColorSpaceProfile {
public:
    ColorSpaceProfile(const float primaries [3][2], const float illuminant[2], const float lumaCoefficients[3], const float whitePointNits): whitePointNits(whitePointNits) {
        memcpy(this->primaries, primaries, sizeof(float)*3*2);
        memcpy(this->illuminant, illuminant, sizeof(float)*2);
        memcpy(this->lumaCoefficients, lumaCoefficients, sizeof(float)*3);
    }

    float primaries[3][2];
    float illuminant[2];
    float lumaCoefficients[3];
    float whitePointNits;
protected:

};

class CmsMatrix {
public:
    CmsMatrix(const vector<vector<float>> primariesXy, const vector<float> whitePoint) {
        vector<vector<float>> mt = GamutRgbToXYZ(primariesXy, whitePoint);
        matrix.resize(9);
        SetVector(mt);
    }

    CmsMatrix(const float primariesXy[3][2], const float whitePoint[2]) {
        vector<vector<float>> mt = GamutRgbToXYZ(primariesXy, whitePoint);
        matrix.resize(9);
        SetVector(mt);
    }

    CmsMatrix(const vector<vector<float>> source) {
        matrix.resize(9);
        SetVector(source);
    }

    ~CmsMatrix() {

    }

    CmsMatrix operator*(const CmsMatrix &other) {
        vector<vector<float>> resultMatrix(3, vector<float>(3, 0.0f));

        for (size_t i = 0; i < 3; ++i) {
            for (size_t j = 0; j < 3; ++j) {
                for (size_t k = 0; k < 3; ++k) {
                    resultMatrix[i][j] += matrix[i * 3 + k] * other.matrix[k * 3 + j];
                }
            }
        }

        return CmsMatrix(resultMatrix);
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

    CmsMatrix inverted() {
        vector<vector<float>> ret(3, std::vector<float>(3, 0.0f));
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                ret[i][j] = matrix[i * 3 + j];
            }
        }
        ret = inverseVector(ret);
        return CmsMatrix(ret);
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

    vector<float> getMatrix() {
        return matrix;
    }

private:
    std::vector<float> matrix;

    void SetVector(const vector<vector<float>> m) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                matrix[i * 3 + j] = m[i][j];
            }
        }

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

static ColorSpaceProfile *rec2020Profile = new ColorSpaceProfile(Rec2020Primaries, IlluminantD65,
                                                                 Rec2020LumaPrimaries,
                                                                 Rec2020WhitePointNits);
static ColorSpaceProfile *rec709Profile = new ColorSpaceProfile(Rec709Primaries, IlluminantD65,
                                                                Rec709LumaPrimaries,
                                                                Rec709WhitePointNits);
static ColorSpaceProfile *displayP3Profile = new ColorSpaceProfile(DisplayP3Primaries,
                                                                   IlluminantD65,
                                                                   DisplayP3LumaPrimaries,
                                                                   DisplayP3WhitePointNits);

#endif //JXLCODER_COLORSPACEPROFILE_H
