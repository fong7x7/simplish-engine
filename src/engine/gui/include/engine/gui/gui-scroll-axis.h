#pragma once

/// @file gui-scroll-axis.h
/// @brief Which way a scroll panel stacks and scrolls.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng {

/// The direction a `GuiScrollPanel` lays its children along and scrolls.
enum class GuiScrollAxis : uint8_t {
  /// A column, scrolling up and down: a settings list.
  VERTICAL,
  /// A row, scrolling left and right: a strip of character cards.
  HORIZONTAL,
};

}  // namespace eng
