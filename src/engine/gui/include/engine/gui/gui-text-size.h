#pragma once

/// @file gui-text-size.h
/// @brief Steps on a theme's type scale.
/// @par Threading
/// Constants only.

#include <array>
#include <cstddef>
#include <cstdint>

namespace eng {

/// A step on the type scale, in logical pixels by the theme.
enum class GuiTextSize : uint8_t {
  /// 11 by default: captions, badges.
  XS,
  /// 12 by default: secondary text.
  SM,
  /// 14 by default: body text and controls.
  MD,
  /// 17 by default: section headings.
  LG,
  /// 22 by default: panel titles.
  XL,
  /// 32 by default: screen titles.
  DISPLAY,
};

/// How many steps the type scale has.
inline constexpr std::size_t GUI_TEXT_SIZE_COUNT = 6;

/// The default pixel sizes for each step.
inline constexpr std::array<float, GUI_TEXT_SIZE_COUNT> GUI_DEFAULT_TEXT_SIZES{
    11.0f, 12.0f, 14.0f, 17.0f, 22.0f, 32.0f};

}  // namespace eng
