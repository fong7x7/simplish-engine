#pragma once

#include <cstdint>

namespace eng {

/// Scissor rectangle for fragment clipping.
struct RhiScissor {
  /// Horizontal origin in pixels.
  int32_t x = 0;
  /// Vertical origin in pixels.
  int32_t y = 0;
  /// Width in pixels.
  uint32_t width = 0;
  /// Height in pixels.
  uint32_t height = 0;
};

}  // namespace eng
