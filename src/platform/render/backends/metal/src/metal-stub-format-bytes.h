#pragma once

#ifdef ENGINE_RENDERER_METAL

#include <cstdint>
#include <engine/render/rhi-types.h>

namespace eng {

/// Bytes per texel for common formats (stub path).
/// Named algorithm — format byte size table.
inline uint32_t stubBytesPerTexel(RhiFormat fmt) {
  switch (fmt) {
    case RhiFormat::UNDEFINED:
    case RhiFormat::B_C7_UNORM:
    case RhiFormat::B_C7_SRGB:
    case RhiFormat::ASTC4X4_UNORM:
    case RhiFormat::ASTC4X4_SRGB:
      return 0;
    case RhiFormat::R8_UNORM:
      return 1;
    case RhiFormat::R_G8_UNORM:
      return 2;
    case RhiFormat::RGB_A8_UNORM:
    case RhiFormat::RGB_A8_SRGB:
    case RhiFormat::BGR_A8_UNORM:
    case RhiFormat::BGR_A8_SRGB:
      return 4;
    case RhiFormat::R16_FLOAT:
      return 2;
    case RhiFormat::R_G16_FLOAT:
    case RhiFormat::R32_FLOAT:
    case RhiFormat::D32_FLOAT:
      return 4;
    case RhiFormat::RGB_A16_FLOAT:
    case RhiFormat::R_G32_FLOAT:
    case RhiFormat::D32_FLOAT_S8_UINT:
      return 8;
    case RhiFormat::R_G_B32_FLOAT:
      return 12;
    case RhiFormat::RGB_A32_FLOAT:
      return 16;
    case RhiFormat::D16_UNORM:
      return 2;
    case RhiFormat::D24_UNORM_S8_UINT:
      return 4;
  }
  return 0;
}

}  // namespace eng

#endif  // ENGINE_RENDERER_METAL
