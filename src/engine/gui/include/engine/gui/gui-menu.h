#pragma once

/// @file gui-menu.h
/// @brief One menu of a menu bar.
/// @par Threading
/// Plain data.

#include "gui-dropdown-item.h"

#include <string>
#include <vector>

namespace eng {

/// A `GuiMenuBar` menu: its title and its rows.
struct GuiMenu {
  /// The title on the bar.
  std::string title{};
  /// Its rows; a row's `on_select` runs after the menu closes.
  std::vector<GuiDropdownItem> items{};
};

}  // namespace eng
