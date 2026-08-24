#pragma once

#include "rhi-core-types.h"

#include <cstdint>

namespace eng {

struct RhiTextureDesc {
  /// Texture width in texels.
  uint32_t width = 1;
  /// Texture height in texels.
  uint32_t height = 1;
  /// Texture depth in texels (1 for 2D textures).
  uint32_t depth = 1;
  /// Number of mip levels (1 = no mipmaps).
  uint32_t mip_levels = 1;
  /// Number of array layers (1 = non-array texture).
  uint32_t array_layers = 1;
  /// Pixel format of the texture.
  RhiFormat format = RhiFormat::RGB_A8_SRGB;
  /// Intended texture usage flags (sampled, render target, etc.).
  RhiTextureUsage usage = RhiTextureUsage::SAMPLED;
  /// Optional debug label shown in GPU profilers.
  const char* debug_name = nullptr;
  /// Optional initial texel data (row-major, tightly packed). Size must match
  /// width × height × bytes-per-texel for `format` (e.g. 4 for RGB_A8_UNORM).
  /// Stub RHI copies this on create; real backends may ignore or upload.
  const void* initial_pixels = nullptr;
};

}  // namespace eng
