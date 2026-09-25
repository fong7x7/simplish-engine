#pragma once

#include "gui-color.h"

#include <optional>

namespace eng {

constexpr int DEFAULT_DROPDOWN_WIDTH = 140;
constexpr int DEFAULT_DROPDOWN_ITEM_HEIGHT = 24;

/// @brief Visual style parameters for dropdown menus.
/// @thread_safety Immutable value type.
struct GuiDropdownStyle {
  /// Background colour of the dropdown panel; unset takes the theme's
  /// menu fill.
  std::optional<GuiColor> bg_color{};
  /// Text colour for items; unset takes the theme's menu text.
  std::optional<GuiColor> text_color{};
  /// Highlight colour for the hovered item; unset takes the theme's
  /// `control_hover`.
  std::optional<GuiColor> hover_color{};
  /// Width of the dropdown in logical pixels.
  int width = DEFAULT_DROPDOWN_WIDTH;
  /// Height of each item row in logical pixels.
  int item_height = DEFAULT_DROPDOWN_ITEM_HEIGHT;
};

}  // namespace eng
