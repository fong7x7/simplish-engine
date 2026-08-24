#pragma once

#include "gui-color.h"

namespace eng {

/// @brief Visual style parameters for GUI buttons.
/// @thread_safety Immutable value type.
struct GuiButtonStyle {
  /// Background fill color (normal state).
  GuiColor bg_color{};
  /// Text label color.
  GuiColor text_color{};
  /// Background fill color when hovered.
  GuiColor hover_color{};
  /// Corner radius for rounded rect (0 = sharp).
  float corner_radius = 0.0f;
};

}  // namespace eng
