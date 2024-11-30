#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

enum class YuvMatrix {
  Bt601,
  Bt709,
  Bt2020,
  Identity,
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
                    const uint8_t *dst,
                    uint32_t dst_stride,
                    uint32_t new_width,
                    uint32_t new_height,
                    uint32_t method,
                    bool premultiply_alpha);

void weave_scale_u16(const uint16_t *src,
                     uintptr_t src_stride,
                     uint32_t width,
                     uint32_t height,
                     const uint16_t *dst,
                     uint32_t new_width,
                     uint32_t new_height,
                     uintptr_t bit_depth,
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

} // extern "C"
