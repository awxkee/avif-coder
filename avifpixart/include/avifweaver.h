#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

extern "C" {

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
