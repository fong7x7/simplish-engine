#pragma once

/// @file gui-elevation.h
/// @brief How far something is raised off what is under it.
/// @par Threading
/// Constants only.

#include <cstddef>
#include <cstdint>

namespace eng {

/// How far a surface is raised, which picks its drop shadow from the
/// theme: nothing, a card, a menu, a dialog.
enum class GuiElevation : uint8_t {
  /// Flat: no shadow.
  NONE,
  /// A card or a raised button.
  LOW,
  /// A menu, a popover, a tooltip.
  MID,
  /// A dialog.
  HIGH,
};

/// How many elevation levels there are.
inline constexpr std::size_t GUI_ELEVATION_COUNT = 4;

}  // namespace eng
