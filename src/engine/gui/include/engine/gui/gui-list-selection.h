#pragma once

/// @file gui-list-selection.h
/// @brief How many rows of a list can be selected.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng {

/// How a list selects.
enum class GuiListSelection : uint8_t {
  /// Nothing: rows are only activated.
  NONE,
  /// One row at a time.
  SINGLE,
  /// Several: shift-click extends from the last row clicked.
  MULTIPLE,
};

}  // namespace eng
