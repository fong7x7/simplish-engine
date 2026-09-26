#pragma once

/// @file gui-check-state.h
/// @brief Whether a checkbox is ticked.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng {

/// A checkbox's state.
enum class GuiCheckState : uint8_t {
  /// Empty.
  UNCHECKED,
  /// Ticked.
  CHECKED,
  /// A dash: some of what it stands for is on — a "select all" over a
  /// partly selected list. Clicking it checks it.
  INDETERMINATE,
};

}  // namespace eng
