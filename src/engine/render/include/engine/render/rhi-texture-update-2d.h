#pragma once

#include "rhi-types.h"

#include <cstdint>

namespace eng {

/// CPU source layout for `RhiDevice::updateTexture2D`.
struct RhiTextureUpdate2D {
  /// Row-major texels starting at the subregion origin.
  const void* pixels = nullptr;
  /// Left edge of the subregion in texels.
  uint32_t offset_x = 0;
  /// Top edge of the subregion in texels.
  uint32_t offset_y = 0;
  /// Subregion width in texels.
  uint32_t width = 0;
  /// Subregion height in texels.
  uint32_t height = 0;
  /// Bytes between consecutive rows in `pixels`; 0 means `width` × bpp for
  /// `format`.
  uint32_t bytes_per_row = 0;
  /// Texel layout of `pixels` (must match the texture’s format).
  RhiFormat format = RhiFormat::UNDEFINED;
};

}  // namespace eng
