#pragma once

/// @file gui-popover-side.h
/// @brief Which side of its anchor a popover opens on.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng {

/// The side of its anchor a popover prefers; it flips to the opposite one
/// when that has more room.
enum class GuiPopoverSide : uint8_t {
  /// Under the anchor: menus, tooltips, dropdowns.
  BELOW,
  /// Over it.
  ABOVE,
  /// To its right: submenus.
  RIGHT,
  /// To its left.
  LEFT,
};

}  // namespace eng
