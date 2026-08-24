#pragma once

namespace eng {

// --- Scroll state ---

/// @thread_safety Main thread only.
struct ScrollState {
  /// Current horizontal scroll offset in pixels.
  float offset_x = 0.0f;
  /// Current vertical scroll offset in pixels.
  float offset_y = 0.0f;
  /// Total scrollable content width in pixels.
  float content_w = 0.0f;
  /// Total scrollable content height in pixels.
  float content_h = 0.0f;
  /// Horizontal scroll velocity for inertial scrolling.
  float velocity_x = 0.0f;
  /// Vertical scroll velocity for inertial scrolling.
  float velocity_y = 0.0f;
};

}  // namespace eng
