#pragma once

#include <cstdint>
#include <engine/render/rhi-types.h>
#include <vector>

namespace eng {

// --- Font atlas ---

/// @thread_safety Main thread only.
struct FontAtlas {
  /// GPU texture handle for this atlas page.
  RhiTextureHandle texture = 0;
  /// Atlas texture width in pixels.
  uint32_t width = 0;
  /// Atlas texture height in pixels.
  uint32_t height = 0;
  /// Current horizontal packing cursor position.
  uint32_t cursor_x = 0;
  /// Current vertical packing cursor position.
  uint32_t cursor_y = 0;
  /// Height of the current row being packed.
  uint32_t row_height = 0;
  /// CPU-side RGBA8 atlas texels (size width × height × 4).
  std::vector<uint8_t> rgba_pixels{};
};

}  // namespace eng
