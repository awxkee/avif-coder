//
// Created by Radzivon Bartoshyk on 14/01/2024.
//

#ifndef JXLCODER_COLORSPACEPROFILE_H
#define JXLCODER_COLORSPACEPROFILE_H

#include <memory>
#include <vector>
#include "Eigen/Eigen"

using namespace std;

static const float Rec709Primaries[3][2] = {{0.640, 0.330},
                                            {0.300, 0.600},
                                            {0.150, 0.060}};

static Eigen::Matrix<float, 3, 2> getRec709Primaries() {
    Eigen::Matrix<float, 3, 2> xf;
    xf << 0.640, 0.330, 0.300, 0.600, 0.150, 0.060;
    return xf;
}

static const float Rec2020Primaries[3][2] = {{0.708, 0.292},
                                             {0.170, 0.797},
                                             {0.131, 0.046}};

static Eigen::Matrix<float, 3, 2> getRec2020Primaries() {
    Eigen::Matrix<float, 3, 2> xf;
    xf << 0.708, 0.292, 0.170, 0.797, 0.131, 0.046;
    return xf;
}

static const Eigen::Matrix3f getBradfordAdaptation() {
    Eigen::Matrix3f M;
    M << 0.8951, 0.2664, -0.1614,
            -0.7502, 1.7135, 0.0367,
            0.0389, -0.0685, 1.0296;

    return M;
}

static const Eigen::Matrix3f getVanKriesAdaptation() {
    Eigen::Matrix3f M;
    M << 0.4002400, 0.7076000, -0.0808100,
            -0.2263000, 1.1653200, 0.0457000,
            0.0000000, 0.0000000, 0.9182200;

    return M;
}

static const float DisplayP3Primaries[3][2] = {{0.740, 0.270},
                                               {0.220, 0.780},
                                               {0.090, -0.090}};

static Eigen::Matrix<float, 3, 2> getDisplayP3Primaries() {
    Eigen::Matrix<float, 3, 2> xf;
    xf << 0.68, 0.32, 0.265, 0.69, 0.15, 0.06;
    return xf;
}

static Eigen::Matrix<float, 3, 2> getDCIP3Primaries() {
    Eigen::Matrix<float, 3, 2> xf;
    xf << 0.680f, 0.320f, 0.265f, 0.690f, 0.150f, 0.060f;
    return xf;
}

static Eigen::Matrix<float, 3, 2> getBT601_525Primaries() {
    Eigen::Matrix<float, 3, 2> xf;
    xf << 0.630, 0.340, 0.310, 0.595, 0.155, 0.070;
    return xf;
}

static Eigen::Matrix<float, 3, 2> getBT601_625Primaries() {
    Eigen::Matrix<float, 3, 2> xf;
    xf << 0.640, 0.330, 0.290, 0.600, 0.150, 0.060;
    return xf;
}

static Eigen::Matrix<float, 3, 2> getAdobeRGBPrimaries() {
    Eigen::Matrix<float, 3, 2> xf;
    xf << 0.6400, 0.3300, 0.2100, 0.7100, 0.1500, 0.0600;
    return xf;
}

static Eigen::Matrix<float, 3, 2> getBT470MPrimaries() {
    Eigen::Matrix<float, 3, 2> xf;
    xf << 0.67, 0.33, 0.21, 0.71, 0.14, 0.08;
    return xf;
}

static Eigen::Vector2f getIlluminantD65() {
    Eigen::Vector2f xf = {0.3127, 0.3290};
    return xf;
}

static Eigen::Vector2f getIlluminantDCI() {
    return {0.3140, 0.3510};
}

static Eigen::Vector2f getIlluminantC() {
    return {0.310, 0.316};
}

static Eigen::Matrix<float, 3, 2> getSRGBPrimaries() {
    Eigen::Matrix<float, 3, 2> m;
    m << 0.640f, 0.330f, 0.300f, 0.600f, 0.150f, 0.060f;
    return m;
}

static Eigen::Vector3f XyToXYZ(const float x, const float y) {
    Eigen::Vector3f ret(0.f, 0.f, 0.f);
    ret[0] = x / y;
    ret[1] = 1.0f;
    ret[2] = (1.0 - x - y) / y;
    return ret;
}

static const Eigen::Matrix3f getPrimariesXYZ(const Eigen::Matrix<float, 3, 2> primariesXy) {
    Eigen::Matrix3f ret;
    ret << XyToXYZ(primariesXy(0, 0), primariesXy(0, 1)),
            XyToXYZ(primariesXy(1, 0), primariesXy(1, 1)),
            XyToXYZ(primariesXy(2, 0), primariesXy(2, 1));
    return ret;
}

static const Eigen::Vector3f getWhitePoint(const Eigen::Vector2f whitePoint) {
    return XyToXYZ(whitePoint[0], whitePoint[1]);
}

static Eigen::Matrix3f
GamutRgbToXYZ(const Eigen::Matrix<float, 3, 2> primariesXy, const Eigen::Vector2f whitePoint) {
    Eigen::Matrix3f xyZMatrix = getPrimariesXYZ(primariesXy);
    const Eigen::Vector3f whiteXyz = getWhitePoint(whitePoint);
    const Eigen::Matrix3f inverted = xyZMatrix.inverse();
    const Eigen::Vector3f s = inverted * whiteXyz;
    Eigen::Matrix3f transposed = xyZMatrix;
    auto row1 = transposed.row(0) * s;
    auto row2 = transposed.row(1) * s;
    auto row3 = transposed.row(2) * s;
    Eigen::Matrix3f conversion = xyZMatrix.array().rowwise() * s.transpose().array();
    return conversion;
}

#endif //JXLCODER_COLORSPACEPROFILE_H
