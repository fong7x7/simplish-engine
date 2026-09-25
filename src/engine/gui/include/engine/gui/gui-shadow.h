#pragma once

/// @file gui-shadow.h
/// @brief A soft drop shadow, as CSS's `box-shadow` describes one.
/// @par Threading
/// Immutable value type.

#include "gui-color.h"

namespace eng {

/// A drop shadow under a (rounded) rect: CSS's `box-shadow: x y blur
/// spread colour`. A zero-alpha colour is no shadow.
struct GuiShadow {
  /// Horizontal offset in logical pixels.
  float offset_x = 0.0f;
  /// Vertical offset in logical pixels; positive is down.
  float offset_y = 0.0f;
  /// How soft the edge is: roughly the width of the fade.
  float blur = 0.0f;
  /// How much larger than the rect the shadow is before blurring.
  float spread = 0.0f;
  /// Colour at the shadow's darkest.
  GuiColor color{0, 0, 0, 0};
};

}  // namespace eng
