#pragma once

/// @file gui-text-role.h
/// @brief What a piece of text is for, which picks its font from the theme.
/// @par Threading
/// Constants only.

#include <cstddef>
#include <cstdint>

namespace eng {

/// The typographic role of text: the theme maps each to a `GuiFont`, so
/// a heading is a heading everywhere and a theme can restyle them all.
enum class GuiTextRole : uint8_t {
  /// Small print: hints, badges, timestamps.
  CAPTION,
  /// Controls: buttons, tabs, field labels.
  LABEL,
  /// Body text: the default.
  BODY,
  /// A section heading.
  HEADING,
  /// A panel or dialog title.
  TITLE,
  /// A screen's headline: a title card, a big score.
  DISPLAY,
};

/// How many roles there are.
inline constexpr std::size_t GUI_TEXT_ROLE_COUNT = 6;

}  // namespace eng
