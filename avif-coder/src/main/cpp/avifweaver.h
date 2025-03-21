#pragma once
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

enum class FfiTrc {
  /// For future use by ITU-T | ISO/IEC
  Reserved,
  /// Rec. ITU-R BT.709-6<br />
  /// Rec. ITU-R BT.1361-0 conventional colour gamut system (historical)<br />
  /// (functionally the same as the values 6, 14 and 15)    <br />
  Bt709 = 1,
  /// Image characteristics are unknown or are determined by the application.<br />
  Unspecified = 2,
  /// Rec. ITU-R BT.470-6 System M (historical)<br />
  /// United States National Television System Committee 1953 Recommendation for transmission standards for color television<br />
  /// United States Federal Communications Commission (2003) Title 47 Code of Federal Regulations 73.682 (a) (20)<br />
  /// Rec. ITU-R BT.1700-0 625 PAL and 625 SECAM<br />
  Bt470M = 4,
  /// Rec. ITU-R BT.470-6 System B, G (historical)<br />
  Bt470Bg = 5,
  /// Rec. ITU-R BT.601-7 525 or 625<br />
  /// Rec. ITU-R BT.1358-1 525 or 625 (historical)<br />
  /// Rec. ITU-R BT.1700-0 NTSC SMPTE 170M (2004)<br />
  /// (functionally the same as the values 1, 14 and 15)<br />
  Bt601 = 6,
  /// SMPTE 240M (1999) (historical)<br />
  Smpte240 = 7,
  /// Linear transfer characteristics<br />
  Linear = 8,
  /// Logarithmic transfer characteristic (100:1 range)<br />
  Log100 = 9,
  /// Logarithmic transfer characteristic (100 * Sqrt( 10 ) : 1 range)<br />
  Log100sqrt10 = 10,
  /// IEC 61966-2-4<br />
  Iec61966 = 11,
  /// Rec. ITU-R BT.1361-0 extended colour gamut system (historical)<br />
  Bt1361 = 12,
  /// IEC 61966-2-1 sRGB or sYCC<br />
  Srgb = 13,
  /// Rec. ITU-R BT.2020-2 (10-bit system)<br />
  /// (functionally the same as the values 1, 6 and 15)<br />
  Bt202010bit = 14,
  /// Rec. ITU-R BT.2020-2 (12-bit system)<br />
  /// (functionally the same as the values 1, 6 and 14)<br />
  Bt202012bit = 15,
  /// SMPTE ST 2084 for 10-, 12-, 14- and 16-bitsystems<br />
  /// Rec. ITU-R BT.2100-0 perceptual quantization (PQ) system<br />
  Smpte2084 = 16,
  /// SMPTE ST 428-1<br />
  Smpte428 = 17,
  /// ARIB STD-B67<br />
  /// Rec. ITU-R BT.2100-0 hybrid log- gamma (HLG) system<br />
  Hlg = 18,
};

enum class ToneMapping {
  Skip,
  Rec2408,
};

enum class YuvMatrix {
  Bt601,
  Bt709,
  Bt2020,
  Identity,
  YCgCo,
};

enum class YuvRange {
  Tv,
  Pc,
};

enum class YuvType {
  Yuv420,
  Yuv422,
  Yuv444,
};

struct FfiProfileData {
  uint8_t *data;
  uintptr_t size;
  uintptr_t capacity;
};

extern "C" {

void weave_yuv8_to_rgba8(const uint8_t *y_plane,
                         uint32_t y_stride,
                         const uint8_t *u_plane,
                         uint32_t u_stride,
                         const uint8_t *v_plane,
                         uint32_t v_stride,
                         uint8_t *rgba,
                         uint32_t rgba_stride,
                         uint32_t width,
                         uint32_t height,
                         YuvRange range,
                         YuvMatrix yuv_matrix,
                         YuvType yuv_type);

void weave_yuv400_to_rgba8(const uint8_t *y_plane,
                           uint32_t y_stride,
                           uint8_t *rgba,
                           uint32_t rgba_stride,
                           uint32_t width,
                           uint32_t height,
                           YuvRange range,
                           YuvMatrix yuv_matrix);

void weave_yuv400_with_alpha_to_rgba8(const uint8_t *y_plane,
                                      uint32_t y_stride,
                                      const uint8_t *a_plane,
                                      uint32_t a_stride,
                                      uint8_t *rgba,
                                      uint32_t rgba_stride,
                                      uint32_t width,
                                      uint32_t height,
                                      YuvRange range,
                                      YuvMatrix yuv_matrix);

void weave_yuv400_p16_to_rgba16(const uint16_t *y_plane,
                                uint32_t y_stride,
                                uint16_t *rgba,
                                uint32_t rgba_stride,
                                uint32_t bit_depth,
                                uint32_t width,
                                uint32_t height,
                                YuvRange range,
                                YuvMatrix yuv_matrix);

void weave_yuv400_p16_with_alpha_to_rgba16(const uint16_t *y_plane,
                                           uint32_t y_stride,
                                           const uint16_t *a_plane,
                                           uint32_t a_stride,
                                           uint16_t *rgba,
                                           uint32_t rgba_stride,
                                           uint32_t bit_depth,
                                           uint32_t width,
                                           uint32_t height,
                                           YuvRange range,
                                           YuvMatrix yuv_matrix);

void weave_yuv8_with_alpha_to_rgba8(const uint8_t *y_plane,
                                    uint32_t y_stride,
                                    const uint8_t *u_plane,
                                    uint32_t u_stride,
                                    const uint8_t *v_plane,
                                    uint32_t v_stride,
                                    const uint8_t *a_plane,
                                    uint32_t a_stride,
                                    uint8_t *rgba,
                                    uint32_t rgba_stride,
                                    uint32_t width,
                                    uint32_t height,
                                    YuvRange range,
                                    YuvMatrix yuv_matrix,
                                    YuvType yuv_type);

void weave_yuv16_to_rgba16(const uint16_t *y_plane,
                           uint32_t y_stride,
                           const uint16_t *u_plane,
                           uint32_t u_stride,
                           const uint16_t *v_plane,
                           uint32_t v_stride,
                           uint16_t *rgba,
                           uint32_t rgba_stride,
                           uint32_t bit_depth,
                           uint32_t width,
                           uint32_t height,
                           YuvRange range,
                           YuvMatrix yuv_matrix,
                           YuvType yuv_type);

void weave_yuv16_with_alpha_to_rgba16(const uint16_t *y_plane,
                                      uint32_t y_stride,
                                      const uint16_t *u_plane,
                                      uint32_t u_stride,
                                      const uint16_t *v_plane,
                                      uint32_t v_stride,
                                      const uint16_t *a_plane,
                                      uint32_t a_stride,
                                      uint16_t *rgba,
                                      uint32_t rgba_stride,
                                      uint32_t bit_depth,
                                      uint32_t width,
                                      uint32_t height,
                                      YuvRange range,
                                      YuvMatrix yuv_matrix,
                                      YuvType yuv_type);

void weave_scale_u8(const uint8_t *src,
                    uint32_t src_stride,
                    uint32_t width,
                    uint32_t height,
                    uint8_t *dst,
                    uint32_t dst_stride,
                    uint32_t new_width,
                    uint32_t new_height,
                    uint32_t method,
                    bool premultiply_alpha);

void weave_scale_u16(const uint16_t *src,
                     uintptr_t src_stride,
                     uint32_t width,
                     uint32_t height,
                     uint16_t *dst,
                     uint32_t new_width,
                     uint32_t new_height,
                     uintptr_t bit_depth,
                     uint32_t method,
                     bool premultiply_alpha);

void weave_scale_f16(const uint16_t *src,
                     uintptr_t src_stride,
                     uint32_t width,
                     uint32_t height,
                     uint16_t *dst,
                     uint32_t new_width,
                     uint32_t new_height,
                     uint32_t method,
                     bool premultiply_alpha);

void weave_rgba_to_yuv(const uint8_t *rgba,
                       uint32_t rgba_stride,
                       uint8_t *y_plane,
                       uint32_t y_stride,
                       uint8_t *u_plane,
                       uint32_t u_stride,
                       uint8_t *v_plane,
                       uint32_t v_stride,
                       uint32_t width,
                       uint32_t height,
                       YuvRange range,
                       YuvMatrix matrix,
                       YuvType yuv_type);

void weave_rgba_to_yuv400(const uint8_t *rgba,
                          uint32_t rgba_stride,
                          uint8_t *y_plane,
                          uint32_t y_stride,
                          uint32_t width,
                          uint32_t height,
                          YuvRange range,
                          YuvMatrix yuv_matrix);

void weave_yuv16_to_rgba_f16(const uint16_t *y_plane,
                             uint32_t y_stride,
                             const uint16_t *u_plane,
                             uint32_t u_stride,
                             const uint16_t *v_plane,
                             uint32_t v_stride,
                             uint16_t *rgba,
                             uint32_t rgba_stride,
                             uint32_t bit_depth,
                             uint32_t width,
                             uint32_t height,
                             YuvRange range,
                             YuvMatrix yuv_matrix,
                             YuvType yuv_type);

void weave_cvt_rgba8_to_rgba_f16(const uint8_t *rgba8,
                                 uint32_t rgba8_stride,
                                 uint16_t *rgba_f16,
                                 uint32_t rgba_f16_stride,
                                 uint32_t width,
                                 uint32_t height);

void weave_premultiply_rgba_f16(uint16_t *rgba_f16,
                                uint32_t rgba_f16_stride,
                                uint32_t width,
                                uint32_t height);

void weave_cvt_rgba8_to_ar30(const uint8_t *rgba8,
                             uint32_t rgba8_stride,
                             uint8_t *ar30,
                             uint32_t ar30_stride,
                             uint32_t width,
                             uint32_t height);

void weave_cvt_rgba16_to_ar30(const uint16_t *rgba16,
                              uint32_t rgba16_stride,
                              uintptr_t bit_depth,
                              uint8_t *ar30,
                              uint32_t ar30_stride,
                              uint32_t width,
                              uint32_t height);

void weave_cvt_rgba16_to_rgba_f16(const uint16_t *rgba16,
                                  uint32_t rgba16_stride,
                                  uintptr_t bit_depth,
                                  uint16_t *rgba_f16,
                                  uint32_t rgba_f16_stride,
                                  uint32_t width,
                                  uint32_t height);

void apply_icc_rgba8(const uint8_t *src_image,
                     uint32_t src_stride,
                     uint8_t *dst_image,
                     uint32_t dst_stride,
                     uint32_t width,
                     uint32_t height,
                     const uint8_t *icc_profile,
                     uint32_t icc_profile_stride);

void apply_icc_rgba16(const uint16_t *src_image,
                      uint32_t src_stride,
                      uint16_t *dst_image,
                      uint32_t dst_stride,
                      uint32_t bit_depth,
                      uint32_t width,
                      uint32_t height,
                      const uint8_t *icc_profile,
                      uint32_t icc_profile_stride);

void free_profile(FfiProfileData wrapper);

FfiProfileData new_dci_p3_profile();

FfiProfileData new_adobe_rgb_profile();

void weave_rgba8_to_yuv8(uint8_t *y_plane,
                         uint32_t y_stride,
                         uint8_t *u_plane,
                         uint32_t u_stride,
                         uint8_t *v_plane,
                         uint32_t v_stride,
                         const uint8_t *rgba,
                         uint32_t rgba_stride,
                         uint32_t width,
                         uint32_t height,
                         YuvRange range,
                         YuvMatrix yuv_matrix,
                         YuvType yuv_type);

void weave_rgba8_to_y08(uint8_t *y_plane,
                        uint32_t y_stride,
                        const uint8_t *rgba,
                        uint32_t rgba_stride,
                        uint32_t width,
                        uint32_t height,
                        YuvRange range,
                        YuvMatrix yuv_matrix);

void apply_tone_mapping_rgba8(uint8_t *image,
                              uint32_t stride,
                              uint32_t width,
                              uint32_t height,
                              const float *primaries,
                              const float *white_point,
                              FfiTrc trc,
                              ToneMapping mapping,
                              float brightness);

void apply_tone_mapping_rgba16(uint16_t *image,
                               uint32_t stride,
                               uint32_t bit_depth,
                               uint32_t width,
                               uint32_t height,
                               const float *primaries,
                               const float *white_point,
                               FfiTrc trc,
                               ToneMapping mapping,
                               float brightness);

}  // extern "C"
