#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

enum class YuvMatrix {
  Bt601 = 0,
  Bt709 = 1,
  Bt2020 = 2,
};

enum class YuvStandardRange {
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
                         uint32_t dst_stride,
                         uint32_t width,
                         uint32_t height,
                         YuvStandardRange range,
                         YuvMatrix matrix,
                         YuvType yuv_type);

void weave_yuv8_with_alpha_to_rgba8(const uint8_t *y_plane,
                                    uint32_t y_stride,
                                    const uint8_t *u_plane,
                                    uint32_t u_stride,
                                    const uint8_t *v_plane,
                                    uint32_t v_stride,
                                    const uint8_t *a_plane,
                                    uint32_t a_stride,
                                    uint8_t *rgba,
                                    uint32_t dst_stride,
                                    uint32_t width,
                                    uint32_t height,
                                    YuvStandardRange range,
                                    YuvMatrix matrix,
                                    YuvType yuv_type);

void weave_scale_u8(const uint8_t *src,
                    uint32_t src_stride,
                    uint32_t width,
                    uint32_t height,
                    const uint8_t *dst,
                    uint32_t dst_stride,
                    uint32_t new_width,
                    uint32_t new_height,
                    uint32_t method);

void weave_scale_u16(const uint16_t *src,
                     uintptr_t src_stride,
                     uint32_t width,
                     uint32_t height,
                     const uint16_t *dst,
                     uint32_t new_width,
                     uint32_t new_height,
                     uintptr_t bit_depth,
                     uint32_t method);

} // extern "C"
