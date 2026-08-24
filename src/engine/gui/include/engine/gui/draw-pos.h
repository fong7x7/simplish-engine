#pragma once

#include "gui-rect.h"

namespace eng {

/// @brief 2D position in logical pixels for draw operations.
/// @thread_safety Immutable value type.
struct DrawPos {
  /// Horizontal position in logical pixels.
  float x = 0.0f;
  /// Vertical position in logical pixels.
  float y = 0.0f;
};

inline DrawPos drawPosInset(const Rect& rect, float inset_x, float inset_y) {
  return {rect.x + inset_x, rect.y + inset_y};
}

}  // namespace eng
