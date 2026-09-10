#pragma once

#include <cstdint>

namespace eng {

/// Scissor rectangle for fragment clipping.
///
/// Measured from the top-left corner of the target, rows counting down, as
/// Metal, DX12 and Vulkan take it. A backend whose API counts from the
/// bottom — OpenGL — turns it over itself.
struct RhiScissor {
  /// Left edge in pixels.
  int32_t x = 0;
  /// Top edge in pixels, counted down from the target's top row.
  int32_t y = 0;
  /// Width in pixels.
  uint32_t width = 0;
  /// Height in pixels.
  uint32_t height = 0;
};

}  // namespace eng
