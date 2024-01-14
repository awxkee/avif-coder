//
// Created by Radzivon Bartoshyk on 14/01/2024.
//

#ifndef JXLCODER_CMSMATH_HPP
#define JXLCODER_CMSMATH_HPP

using namespace std;

#include <vector>

static vector<float> add(const vector<float>& vec, const float scalar) {
    vector<float> result(vec.size());
    copy(vec.begin(), vec.end(), result.begin());
    for (float& element : result) {
        element += scalar;
    }
    return result;
}

static vector<float> mul(const vector<float>& vec, const float scalar) {
    vector<float> result(vec.size());
    copy(vec.begin(), vec.end(), result.begin());
    for (float& element : result) {
        element *= scalar;
    }
    return result;
}

static vector<float> mul(const vector<vector<float>>& matrix, const vector<float>& vector) {
    if (matrix.size() != 3 || matrix[0].size() != 3 || vector.size() != 3) {
        throw std::invalid_argument("Matrix must be 3x3 and vector must have size 3");
    }

    std::vector<float> result(3, 0.0f);

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            result[i] += matrix[i][j] * vector[j];
        }
    }

    return result;
}

static vector<float> mul(const vector<float>& vector1, const vector<float>& vector2) {
    if (vector1.size() != 3 || vector2.size() != 3) {
        // Check for valid dimensions
        throw std::invalid_argument("Vectors must be of size 3.");
    }

    std::vector<float> result(3);

    for (int i = 0; i < 3; i++) {
        result[i] = vector1[i] * vector2[i];
    }

    return result;
}

static vector<vector<float>> mul(const vector<vector<float>>& matrix1, const vector<vector<float>>& matrix2) {
    auto rows1 = matrix1.size();
    auto cols1 = matrix1[0].size();
    auto rows2 = matrix2.size();
    auto cols2 = matrix2[0].size();

    if (cols1 != rows2) {
        // Check if the number of columns in the first matrix is equal to the number of rows in the second matrix
        throw std::invalid_argument("Number of columns in the first matrix must be equal to the number of rows in the second matrix.");
    }

    std::vector<std::vector<float>> result(rows1, std::vector<float>(cols2, 0.0f));

    for (int i = 0; i < rows1; ++i) {
        for (int j = 0; j < cols2; ++j) {
            for (int k = 0; k < cols1; ++k) {
                result[i][j] += matrix1[i][k] * matrix2[k][j];
            }
        }
    }

    return result;
}

#endif //JXLCODER_CMSMATH_HPP
