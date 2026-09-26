#pragma once

/// @file gui-font.h
/// @brief How a run of text is set: size, weight, slant, spacing.
/// @par Threading
/// Immutable value type.

#include "font-face.h"
#include "gui-digits.h"
#include "gui-font-slant.h"

#include <cstdint>

namespace eng {

/// Default text size in layout pixels; the editor's chrome is tuned to it.
inline constexpr float GUI_DEFAULT_TEXT_SIZE = 14.0f;

/// How text is set — CSS's `font-size`, `font-weight`, `font-style`,
/// `line-height`, `letter-spacing` and `font-variant-numeric`. The face is
/// the loaded one nearest `weight` and `slant`; see `GuiTheme::font` for
/// a role's.
struct GuiFont {
  /// Size in layout pixels.
  float size = GUI_DEFAULT_TEXT_SIZE;
  /// CSS weight, 100–900.
  uint16_t weight = FONT_WEIGHT_NORMAL;
  /// Upright or italic.
  GuiFontSlant slant = GuiFontSlant::NORMAL;
  /// Baseline to baseline as a multiple of `size`; 0 for the font's own.
  float line_height = 0.0f;
  /// Extra space after every character, in layout pixels; negative
  /// tightens display type.
  float letter_spacing = 0.0f;
  /// Proportional or tabular digits.
  GuiDigits digits = GuiDigits::PROPORTIONAL;
};

}  // namespace eng
