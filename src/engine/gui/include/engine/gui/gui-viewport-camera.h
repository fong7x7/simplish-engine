#pragma once

// Orbit / pan camera state shared by GuiViewport and editor UI state.

namespace eng {

constexpr float DEFAULT_CAMERA_YAW = 0.4f;
constexpr float DEFAULT_CAMERA_PITCH = -0.6f;
constexpr float DEFAULT_CAMERA_DISTANCE = 300.0f;

/// Viewport camera orbit state.
/// @thread_safety Main thread only.
struct ViewportCamera {
  /// Horizontal orbit angle in radians.
  float yaw = DEFAULT_CAMERA_YAW;
  /// Vertical orbit angle in radians (negative = above, clamped).
  float pitch = DEFAULT_CAMERA_PITCH;
  /// Distance from orbit target.
  float distance = DEFAULT_CAMERA_DISTANCE;
  /// Target X position (pan).
  float target_x = 0.0f;
  /// Target Y position (pan).
  float target_y = 0.0f;
};

}  // namespace eng
