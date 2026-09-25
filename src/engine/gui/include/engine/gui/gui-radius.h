#pragma once

/// @file gui-radius.h
/// @brief Steps on a theme's corner-radius scale.
/// @par Threading
/// Constants only.

#include <array>
#include <cstddef>
#include <cstdint>

namespace eng {

/// A step on the corner-radius scale.
enum class GuiRadius : uint8_t {
  /// Square corners.
  NONE,
  /// 3 by default: fields and small controls.
  SM,
  /// 5 by default: buttons.
  MD,
  /// 8 by default: cards and menus.
  LG,
  /// Half the shorter side: a pill or a circle.
  PILL,
};

/// How many steps the radius scale has.
inline constexpr std::size_t GUI_RADIUS_COUNT = 5;

/// Radius large enough that any rect it is drawn on becomes a pill.
inline constexpr float GUI_RADIUS_PILL = 9999.0f;

/// The default pixels for each step.
inline constexpr std::array<float, GUI_RADIUS_COUNT> GUI_DEFAULT_RADII{
    0.0f, 3.0f, 5.0f, 8.0f, GUI_RADIUS_PILL};

}  // namespace eng
