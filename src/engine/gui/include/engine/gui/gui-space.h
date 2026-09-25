#pragma once

/// @file gui-space.h
/// @brief Steps on a theme's spacing scale.
/// @par Threading
/// Constants only.

#include <array>
#include <cstddef>
#include <cstdint>

namespace eng {

/// A step on the spacing scale, for padding, margins and gaps:
/// `theme.space(GuiSpace::MD)` rather than a pixel count, so every gap in
/// the UI is one of a few sizes and a theme can tighten or loosen them all.
enum class GuiSpace : uint8_t {
  /// 0.
  NONE,
  /// 2 by default.
  XXS,
  /// 4 by default.
  XS,
  /// 8 by default.
  SM,
  /// 12 by default.
  MD,
  /// 16 by default.
  LG,
  /// 24 by default.
  XL,
  /// 32 by default.
  XXL,
};

/// How many steps the spacing scale has.
inline constexpr std::size_t GUI_SPACE_COUNT = 8;

/// The default pixels for each step.
inline constexpr std::array<float, GUI_SPACE_COUNT> GUI_DEFAULT_SPACING{
    0.0f, 2.0f, 4.0f, 8.0f, 12.0f, 16.0f, 24.0f, 32.0f};

}  // namespace eng
