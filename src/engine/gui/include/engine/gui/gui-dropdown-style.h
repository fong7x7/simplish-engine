#pragma once

#include "gui-color.h"

namespace eng {

constexpr int DEFAULT_DROPDOWN_WIDTH = 140;
constexpr int DEFAULT_DROPDOWN_ITEM_HEIGHT = 24;

/// @brief Visual style parameters for dropdown menus.
/// @thread_safety Immutable value type.
struct GuiDropdownStyle {
  /// Background color of the dropdown panel.
  GuiColor bg_color{};
  /// Text color for items.
  GuiColor text_color{};
  /// Highlight color for the hovered item.
  GuiColor hover_color{};
  /// Width of the dropdown in logical pixels.
  int width = DEFAULT_DROPDOWN_WIDTH;
  /// Height of each item row in logical pixels.
  int item_height = DEFAULT_DROPDOWN_ITEM_HEIGHT;
};

}  // namespace eng
