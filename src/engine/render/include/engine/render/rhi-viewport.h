#pragma once

namespace eng {

/// Viewport rectangle with depth range for rasterisation.
struct RhiViewport {
  /// Horizontal origin in pixels.
  float x = 0.0f;
  /// Vertical origin in pixels.
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
