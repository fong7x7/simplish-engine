#pragma once

/// @file gui-button-variant.h
/// @brief Which of the theme's button looks a button takes.
/// @par Threading
/// Constants only.

#include <cstddef>
#include <cstdint>

namespace eng {

/// A button's role, which picks its look from the theme.
enum class GuiButtonVariant : uint8_t {
  /// An ordinary action.
  NEUTRAL,
  /// The main action on a screen: filled with the accent.
  PRIMARY,
  /// A destructive action: delete, discard.
  DANGER,
  /// No box until hovered: menu titles, toolbar icons.
  GHOST,
};

/// How many variants there are.
inline constexpr std::size_t GUI_BUTTON_VARIANT_COUNT = 4;

}  // namespace eng
