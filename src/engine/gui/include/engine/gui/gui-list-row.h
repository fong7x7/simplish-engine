#pragma once

/// @file gui-list-row.h
/// @brief One visible row of a virtual list, as its drawing callback sees it.
/// @par Threading
/// Plain data.

#include "gui-rect.h"

#include <cstddef>

namespace eng {

/// A row `GuiVirtualList` asks its `draw_row` to paint.
struct GuiListRow {
  /// Which row of the data.
  std::size_t index = 0;
  /// Where it is on screen.
  Rect rect{};
  /// Whether it is selected.
  bool selected = false;
  /// Whether the pointer is on it.
  bool hovered = false;
  /// Whether it is the row navigation is on.
  bool current = false;
};

}  // namespace eng
