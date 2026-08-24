#pragma once

namespace eng {

/// @thread_safety Main thread only.
struct Edges {
  /// Top edge inset in pixels.
  float top = 0.0f;
  /// Right edge inset in pixels.
  float right = 0.0f;
  /// Bottom edge inset in pixels.
  float bottom = 0.0f;
  /// Left edge inset in pixels.
  float left = 0.0f;
};

}  // namespace eng
