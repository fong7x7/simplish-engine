#pragma once

/// @file gui-mouse-event.h
/// @brief Mouse event for widget routing and handler dispatch.
/// @par Threading Main thread only.

#include "gui-mouse-event-type.h"

#include <cstdint>

namespace eng {

/// @brief Mouse button identifier.
/// @thread_safety Immutable value type.
enum class GuiMouseButton : uint8_t {
  /// Left mouse button.
  LEFT,
  /// Middle mouse button (scroll wheel click).
  MIDDLE,
  /// Right mouse button.
  RIGHT,
};

/// @brief Mouse event for widget routing and handler dispatch.
/// @thread_safety Immutable value type.
struct GuiMouseEvent {
  /// Type of mouse event (move, button down/up, double-click, scroll).
  GuiMouseEventType type = GuiMouseEventType::MOVE;
  /// Mouse X in window-logical coordinates.
  float x = 0.0f;
  /// Mouse Y in window-logical coordinates.
  float y = 0.0f;
  /// Which button triggered the event (ignored for move events).
  GuiMouseButton button = GuiMouseButton::LEFT;
  /// Whether the Shift modifier key is held.
  bool shift_held = false;
  /// Horizontal scroll delta (only for SCROLL type).
  float scroll_dx = 0.0f;
  /// Vertical scroll delta (only for SCROLL type).
  float scroll_dy = 0.0f;
};

}  // namespace eng
