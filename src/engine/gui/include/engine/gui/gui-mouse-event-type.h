#pragma once

#include <cstdint>

namespace eng {

/// @brief Type of mouse event.
/// @thread_safety Immutable value type.
enum class GuiMouseEventType : uint8_t {
  MOVE,
  BUTTON_DOWN,
  BUTTON_UP,
  DOUBLE_CLICK,
  SCROLL,
};

}  // namespace eng
