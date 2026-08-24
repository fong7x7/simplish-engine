#pragma once

/// @file gui-scroll-event.h
/// @brief Wheel/scroll event for `GuiWidget::handleScroll`.
/// @par Threading Main thread only.

namespace eng {

/// @brief Wheel/scroll event for widget scroll handling.
/// @thread_safety Immutable value type.
struct GuiScrollEvent {
  /// Mouse X at time of scroll in window-logical coordinates.
  float x = 0.0f;
  /// Mouse Y at time of scroll in window-logical coordinates.
  float y = 0.0f;
  /// Vertical scroll delta (positive = up/away from user).
  float delta_y = 0.0f;
};

}  // namespace eng
