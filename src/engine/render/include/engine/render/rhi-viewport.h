#pragma once

namespace eng {

/// Viewport rectangle with depth range for rasterisation.
///
/// Measured from the top-left corner of the target, rows counting down, as
/// Metal, DX12 and Vulkan take it. A backend whose API counts from the
/// bottom — OpenGL — turns it over itself.
struct RhiViewport {
  /// Left edge in pixels.
  float x = 0.0f;
  /// Top edge in pixels, counted down from the target's top row.
  float y = 0.0f;
  /// Width in pixels.
  float width = 0.0f;
  /// Height in pixels.
  float height = 0.0f;
  /// Minimum depth range.
  float min_depth = 0.0f;
  /// Maximum depth range.
  float max_depth = 1.0f;
};

}  // namespace eng
