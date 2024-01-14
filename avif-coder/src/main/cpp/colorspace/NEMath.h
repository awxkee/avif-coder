//
//  NEMath.h
//  avif.swift [https://github.com/awxkee/avif.swift]
//
//  Created by Radzivon Bartoshyk on 08/10/2023.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//

#ifndef NEMath_h
#define NEMath_h

#ifdef __cplusplus

#if __arm64__

#include <arm_neon.h>
#include <vector>
#include <array>

#if defined(__clang__)
#pragma clang fp contract(fast) exceptions(ignore) reassociate(on)
#endif

#if defined(__GNUC__) && defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
#define FP16_FULL_SUPPORTED 1
#endif

static inline float32x4_t vltq_n_f32(const float32x4_t& inputVector, const float ifValue, const float newValue);

/* Logarithm polynomial coefficients */
static const std::array<float32x4_t, 8> log_tab =
{
    {
        vdupq_n_f32(-2.29561495781f),
        vdupq_n_f32(-2.47071170807f),
        vdupq_n_f32(-5.68692588806f),
        vdupq_n_f32(-0.165253549814f),
        vdupq_n_f32(5.17591238022f),
        vdupq_n_f32(0.844007015228f),
        vdupq_n_f32(4.58445882797f),
        vdupq_n_f32(0.0141278216615f),
    }
};

/* Log10 polynomial coefficients */
static const std::array<float32x4_t, 8> log10Coeffs =
{
    {
        vdupq_n_f32(-0.99697286229624f),
        vdupq_n_f32(-1.07301643912502f),
        vdupq_n_f32(-2.46980061535534f),
        vdupq_n_f32(-0.07176870463131f),
        vdupq_n_f32(2.247870219989470f),
        vdupq_n_f32(0.366547581117400f),
        vdupq_n_f32(1.991005185100089f),
        vdupq_n_f32(0.006135635201050f),
    }
};

static const uint32_t exp_f32_coeff[] =
{
    0x3f7ffff6, // x^1: 0x1.ffffecp-1f
    0x3efffedb, // x^2: 0x1.fffdb6p-2f
    0x3e2aaf33, // x^3: 0x1.555e66p-3f
    0x3d2b9f17, // x^4: 0x1.573e2ep-5f
    0x3c072010, // x^5: 0x1.0e4020p-7f
};

__attribute__((always_inline))
__attribute__((flatten))
static inline float32x4_t vtaylor_polyq_f32(float32x4_t x, const std::array<float32x4_t, 8> &coeffs)
{
    float32x4_t A   = vmlaq_f32(coeffs[0], coeffs[4], x);
    float32x4_t B   = vmlaq_f32(coeffs[2], coeffs[6], x);
    float32x4_t C   = vmlaq_f32(coeffs[1], coeffs[5], x);
    float32x4_t D   = vmlaq_f32(coeffs[3], coeffs[7], x);
    float32x4_t x2  = vmulq_f32(x, x);
    float32x4_t x4  = vmulq_f32(x2, x2);
    float32x4_t res = vmlaq_f32(vmlaq_f32(A, B, x2), vmlaq_f32(C, D, x2), x4);
    return res;
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float32x4_t prefer_vfmaq_f32(float32x4_t a, float32x4_t b, float32x4_t c)
{
#if __ARM_FEATURE_FMA
    return vfmaq_f32(a, b, c);
#else // __ARM_FEATURE_FMA
    return vmlaq_f32(a, b, c);
#endif // __ARM_FEATURE_FMA
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float32x4_t vexpq_f32(float32x4_t x)
{
    const auto c1 = vreinterpretq_f32_u32(vdupq_n_u32(exp_f32_coeff[0]));
    const auto c2 = vreinterpretq_f32_u32(vdupq_n_u32(exp_f32_coeff[1]));
    const auto c3 = vreinterpretq_f32_u32(vdupq_n_u32(exp_f32_coeff[2]));
    const auto c4 = vreinterpretq_f32_u32(vdupq_n_u32(exp_f32_coeff[3]));
    const auto c5 = vreinterpretq_f32_u32(vdupq_n_u32(exp_f32_coeff[4]));

    const auto shift      = vreinterpretq_f32_u32(vdupq_n_u32(0x4b00007f)); // 2^23 + 127 = 0x1.0000fep23f
    const auto inv_ln2    = vreinterpretq_f32_u32(vdupq_n_u32(0x3fb8aa3b)); // 1 / ln(2) = 0x1.715476p+0f
    const auto neg_ln2_hi = vreinterpretq_f32_u32(vdupq_n_u32(0xbf317200)); // -ln(2) from bits  -1 to -19: -0x1.62e400p-1f
    const auto neg_ln2_lo = vreinterpretq_f32_u32(vdupq_n_u32(0xb5bfbe8e)); // -ln(2) from bits -20 to -42: -0x1.7f7d1cp-20f

    const auto inf       = vdupq_n_f32(std::numeric_limits<float>::infinity());
    const auto max_input = vdupq_n_f32(88.37f); // Approximately ln(2^127.5)
    const auto zero      = vdupq_n_f32(0.f);
    const auto min_input = vdupq_n_f32(-86.64f); // Approximately ln(2^-125)

    // Range reduction:
    //   e^x = 2^n * e^r
    // where:
    //   n = floor(x / ln(2))
    //   r = x - n * ln(2)
    //
    // By adding x / ln(2) with 2^23 + 127 (shift):
    //   * As FP32 fraction part only has 23-bits, the addition of 2^23 + 127 forces decimal part
    //     of x / ln(2) out of the result. The integer part of x / ln(2) (i.e. n) + 127 will occupy
    //     the whole fraction part of z in FP32 format.
    //     Subtracting 2^23 + 127 (shift) from z will result in the integer part of x / ln(2)
    //     (i.e. n) because the decimal part has been pushed out and lost.
    //   * The addition of 127 makes the FP32 fraction part of z ready to be used as the exponent
    //     in FP32 format. Left shifting z by 23 bits will result in 2^n.
    const auto z     = prefer_vfmaq_f32(shift, x, inv_ln2);
    const auto n     = z - shift;
    const auto scale = vreinterpretq_f32_u32(vreinterpretq_u32_f32(z) << 23); // 2^n

    // The calculation of n * ln(2) is done using 2 steps to achieve accuracy beyond FP32.
    // This outperforms longer Taylor series (3-4 tabs) both in term of accuracy and performance.
    const auto r_hi = prefer_vfmaq_f32(x, n, neg_ln2_hi);
    const auto r    = prefer_vfmaq_f32(r_hi, n, neg_ln2_lo);

    // Compute the truncated Taylor series of e^r.
    //   poly = scale * (1 + c1 * r + c2 * r^2 + c3 * r^3 + c4 * r^4 + c5 * r^5)
    const auto r2 = r * r;

    const auto p1     = c1 * r;
    const auto p23    = prefer_vfmaq_f32(c2, c3, r);
    const auto p45    = prefer_vfmaq_f32(c4, c5, r);
    const auto p2345  = prefer_vfmaq_f32(p23, p45, r2);
    const auto p12345 = prefer_vfmaq_f32(p1, p2345, r2);

    auto poly = prefer_vfmaq_f32(scale, p12345, scale);

    // Handle underflow and overflow.
    poly = vbslq_f32(vcltq_f32(x, min_input), zero, poly);
    poly = vbslq_f32(vcgtq_f32(x, max_input), inf, poly);

    return poly;
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float32x4_t vlogq_f32(float32x4_t x)
{
    static const int32x4_t   CONST_127 = vdupq_n_s32(127);           // 127
    static const float32x4_t CONST_LN2 = vdupq_n_f32(0.6931471805f); // ln(2)

    // Extract exponent
    int32x4_t   m   = vsubq_s32(vreinterpretq_s32_u32(vshrq_n_u32(vreinterpretq_u32_f32(x), 23)), CONST_127);
    float32x4_t val = vreinterpretq_f32_s32(vsubq_s32(vreinterpretq_s32_f32(x), vshlq_n_s32(m, 23)));

    // Polynomial Approximation
    float32x4_t poly = vtaylor_polyq_f32(val, log_tab);

    // Reconstruct
    poly = vmlaq_f32(poly, vcvtq_f32_s32(m), CONST_LN2);

    return poly;
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float32x4_t vlog10q_f32(float32x4_t x)
{
    static const int32x4_t   CONST_127 = vdupq_n_s32(127);           // 127
    static const float32x4_t CONST_LOG10 = vdupq_n_f32(0.3010299957f); // log(2)

    // Extract exponent
    int32x4_t   m   = vsubq_s32(vreinterpretq_s32_u32(vshrq_n_u32(vreinterpretq_u32_f32(x), 23)), CONST_127);
    float32x4_t val = vreinterpretq_f32_s32(vsubq_s32(vreinterpretq_s32_f32(x), vshlq_n_s32(m, 23)));

    // Polynomial Approximation
    float32x4_t poly = vtaylor_polyq_f32(val, log10Coeffs);

    // Reconstruct
    poly = vmlaq_f32(poly, vcvtq_f32_s32(m), CONST_LOG10);

    return poly;
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float32x4_t vpowq_f32(float32x4_t val, float32x4_t n)
{
    return vexpq_f32(vmulq_f32(n, vlogq_f32(val)));
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float32x4_t vpowq_f32(float32x4_t t, float power) {
    return vpowq_f32(t, vdupq_n_f32(power));
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float32x4_t vclampq_n_f32(const float32x4_t t, const float min, const float max) {
    return vmaxq_f32(vminq_f32(t, vdupq_n_f32(max)), vdupq_n_f32(min));
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float16x8_t vclampq_n_f16(const float16x8_t t, const float16_t min, const float16_t max) {
#if FP16_FULL_SUPPORTED
    return vmaxq_f16(vminq_f16(t, vdupq_n_f16(max)), vdupq_n_f16(min));
#else
    const float vMin = min;
    const float vMax = max;
    const float32x4_t low = vclampq_n_f32(vcvt_f32_f16(vget_low_f16(t)), vMin, vMax);
    const float32x4_t high = vclampq_n_f32(vcvt_f32_f16(vget_high_f16(t)), vMin, vMax);
    return vcombine_f16(vcvt_f16_f32(low), vcvt_f16_f32(high));
#endif
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float32x4x4_t vtransposeq_f32(const float32x4x4_t matrix)
{
    float32x4_t     row0 = matrix.val[0];
    float32x4_t     row1 = matrix.val[1];
    float32x4_t     row2 = matrix.val[2];
    float32x4_t     row3 = matrix.val[3];

    float32x4x2_t   row01 = vtrnq_f32(row0, row1);
    float32x4x2_t   row23 = vtrnq_f32(row2, row3);

    float32x4x4_t r = {
        vcombine_f32(vget_low_f32(row01.val[0]), vget_low_f32(row23.val[0])),
        vcombine_f32(vget_low_f32(row01.val[1]), vget_low_f32(row23.val[1])),
        vcombine_f32(vget_high_f32(row01.val[0]), vget_high_f32(row23.val[0])),
        vcombine_f32(vget_high_f32(row01.val[1]), vget_high_f32(row23.val[1]))
    };
    return r;
}

__attribute__((always_inline))
__attribute__((flatten))
static inline uint8x8x4_t vtranspose_u8(const uint8x8x4_t matrix) {
    const uint8x8x2_t b0 = vtrn_u8(matrix.val[0], matrix.val[1]);
    const uint8x8x2_t b1 = vtrn_u8(matrix.val[0], matrix.val[1]);


    const uint16x4x2_t c0 =
        vtrn_u16(vreinterpret_u16_u8(b0.val[0]), vreinterpret_u16_u8(b1.val[0]));
    const uint16x4x2_t c1 =
        vtrn_u16(vreinterpret_u16_u8(b0.val[1]), vreinterpret_u16_u8(b1.val[1]));

    uint8x8_t a0 = vreinterpret_u8_u16(c0.val[0]);
    uint8x8_t a1 = vreinterpret_u8_u16(c1.val[0]);
    uint8x8_t a2 = vreinterpret_u8_u16(c0.val[1]);
    uint8x8_t a3 = vreinterpret_u8_u16(c1.val[1]);
    uint8x8x4_t r = {a0, a1, a2, a3};
    return r;
}

__attribute__((always_inline))
__attribute__((flatten))
static inline uint8x16x4_t vtransposeq_u8(const uint8x16x4_t matrix) {
    const uint8x16x2_t b0 = vtrnq_u8(matrix.val[0], matrix.val[1]);
    const uint8x16x2_t b1 = vtrnq_u8(matrix.val[2], matrix.val[3]);

    const uint16x8x2_t c0 = vtrnq_u16(vreinterpretq_u16_u8(b0.val[0]),
                                      vreinterpretq_u16_u8(b1.val[0]));
    const uint16x8x2_t c1 = vtrnq_u16(vreinterpretq_u16_u8(b0.val[1]),
                                      vreinterpretq_u16_u8(b1.val[1]));

    const uint32x4x2_t d0 = vtrnq_u32(vreinterpretq_u32_u16(c0.val[0]),
                                      vreinterpretq_u32_u16(c1.val[0]));
    const uint32x4x2_t d1 = vtrnq_u32(vreinterpretq_u32_u16(c0.val[1]),
                                      vreinterpretq_u32_u16(c1.val[1]));

    uint8x16x2_t dc0;
    dc0.val[0] = vreinterpretq_u8_u32(d0.val[0]);
    dc0.val[1] = vreinterpretq_u8_u32(d0.val[1]);

    uint8x16x2_t dc1;
    dc1.val[0] = vreinterpretq_u8_u32(d1.val[0]);
    dc1.val[1] = vreinterpretq_u8_u32(d1.val[1]);

    uint8x16x4_t rs = {
        vcombine_u8(vget_low_u8(dc0.val[0]), vget_low_u8(dc1.val[0])),
        vcombine_u8(vget_low_u8(dc0.val[1]), vget_low_u8(dc1.val[1])),
        vcombine_u8(vget_high_u8(dc0.val[0]), vget_high_u8(dc1.val[0])),
        vcombine_u8(vget_high_u8(dc0.val[1]), vget_high_u8(dc1.val[1])),
    };
    return rs;
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float16x8x4_t vtransposeq_f16(const float16x8x4_t matrix)
{
#if FP16_FULL_SUPPORTED
    float16x8x2_t r1 = vtrnq_f16(matrix.val[0], matrix.val[1]);
    float16x8x2_t r2 = vtrnq_f16(matrix.val[2], matrix.val[3]);

    const uint32x4x2_t c0 = vtrnq_u32(vreinterpretq_u32_u16(vreinterpretq_u16_f16(r1.val[0])),
                                      vreinterpretq_u32_u16(vreinterpretq_u16_f16(r2.val[0])));
    const uint32x4x2_t c1 = vtrnq_u32(vreinterpretq_u32_u16(vreinterpretq_u16_f16(r1.val[1])),
                                      vreinterpretq_u32_u16(vreinterpretq_u16_f16(r2.val[1])));

    float16x8_t rw1 = vreinterpretq_f16_u16(vreinterpretq_u16_u32(c0.val[0]));
    float16x8_t rw2 = vreinterpretq_f16_u16(vreinterpretq_u16_u32(c0.val[1]));
    float16x8_t rw3 = vreinterpretq_f16_u16(vreinterpretq_u16_u32(c1.val[0]));
    float16x8_t rw4 = vreinterpretq_f16_u16(vreinterpretq_u16_u32(c1.val[1]));

    float16x8x4_t transposed = {
        rw1, rw3, rw2, rw4
    };

    return transposed;
#else
    uint16x8x2_t r1 = vtrnq_u16(vreinterpretq_u16_f16(matrix.val[0]), vreinterpretq_u16_f16(matrix.val[1]));
    uint16x8x2_t r2 = vtrnq_u16(vreinterpretq_u16_f16(matrix.val[2]), vreinterpretq_u16_f16(matrix.val[3]));

    const uint32x4x2_t c0 = vtrnq_u32(vreinterpretq_u32_u16(r1.val[0]),
                                      vreinterpretq_u32_u16(r2.val[0]));
    const uint32x4x2_t c1 = vtrnq_u32(vreinterpretq_u32_u16(r1.val[1]),
                                      vreinterpretq_u32_u16(r2.val[1]));

    float16x8_t rw1 = vreinterpretq_f16_u16(vreinterpretq_u16_u32(c0.val[0]));
    float16x8_t rw2 = vreinterpretq_f16_u16(vreinterpretq_u16_u32(c0.val[1]));
    float16x8_t rw3 = vreinterpretq_f16_u16(vreinterpretq_u16_u32(c1.val[0]));
    float16x8_t rw4 = vreinterpretq_f16_u16(vreinterpretq_u16_u32(c1.val[1]));

    float16x8x4_t transposed = {
        rw1, rw3, rw2, rw4
    };

    return transposed;
#endif
}

__attribute__((always_inline))
__attribute__((flatten))
static inline uint32x4_t vhtonlq_u32(const uint32x4_t hostlong) {
    uint8x8_t low = vreinterpret_u8_u32(vget_low_u32(hostlong));
    uint8x8_t high = vreinterpret_u8_u32(vget_high_u32(hostlong));

    low = vrev32_u8(low); // Swap bytes within low 32-bit elements
    high = vrev32_u8(high); // Swap bytes within high 32-bit elements

    uint32x4_t result = vcombine_u32(vreinterpret_u32_u8(low), vreinterpret_u32_u8(high));
    return result;
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float32x4_t vcopysignq_f32(const float32x4_t dst, const float32x4_t src) {
    // Create a mask where each element is 1 if the sign is negative, otherwise 0
    uint32x4_t mask = vcltq_f32(src, vdupq_n_f32(0.0f));
    return vbslq_f32(mask, vnegq_f32(dst), dst);
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float32x2_t vsumq_f32x2(const float32x4_t v, const float32x4_t v1) {
    //    float32x2_t r = vadd_f32(vget_high_f32(v), vget_low_f32(v));
    //    float32x2_t r1 = vadd_f32(vget_high_f32(v1), vget_low_f32(v1));
    //    r = vpadd_f32(r, r);
    //    r1 = vpadd_f32(r1, r1);
    //    r = vext_f32(r, vdup_n_f32(0), 1);
    //    r1 = vext_f32(vdup_n_f32(0), r1, 1);
    //    return vadd_f32(r, r1);
    float32x2_t r = { vaddvq_f32(v), vaddvq_f32(v1) };
    return r;
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float32x4_t vsumq_f32x4(const float32x4_t v, const float32x4_t v1, const float32x4_t v2, const float32x4_t v3) {
    float32x4_t r = { vaddvq_f32(v), vaddvq_f32(v1), vaddvq_f32(v2), vaddvq_f32(v3) };
    return r;
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float32x4_t vsetq_if_f32(const float32x4_t& inputVector, const float ifValue, const float newValue) {
    const float32x4_t ones = vdupq_n_f32(newValue);
    const uint32x4_t zeroMask = vceqq_f32(inputVector, vdupq_n_f32(ifValue));
    return vbslq_f32(zeroMask, ones, inputVector);
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float32x4_t vgtq_n_f32(const float32x4_t& inputVector, const float ifValue, const float newValue) {
    const float32x4_t replace = vdupq_n_f32(newValue);
    const uint32x4_t zeroMask = vcgtq_f32(inputVector, vdupq_n_f32(ifValue));
    return vbslq_f32(zeroMask, replace, inputVector);
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float32x4_t vltq_n_f32(const float32x4_t& inputVector, const float ifValue, const float newValue) {
    const float32x4_t replace = vdupq_n_f32(newValue);
    const uint32x4_t zeroMask = vcltq_f32(inputVector, vdupq_n_f32(ifValue));
    return vbslq_f32(zeroMask, replace, inputVector);
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float vsumq_f16(const float16x8_t v) {
    const float32x4_t low = vcvt_f32_f16(vget_low_f16(v));
    const float32x4_t high = vcvt_f32_f16(vget_high_f16(v));
    return vaddvq_f32(vaddq_f32(high, low));
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float16x8_t vdupq_n_f16_f32(const float v) {
    return vcombine_f16(vcvt_f16_f32(vdupq_n_f32(v)), vcvt_f16_f32(vdupq_n_f32(v)));
}

__attribute__((always_inline))
__attribute__((flatten))
static inline float vdot_f32(const float32x4_t v, const float32x4_t r) {
    return vaddvq_f32(vmulq_f32(v, r));
}

#endif

#endif // _cplusplus

#endif /* NEMath_h */
