#pragma once

/// @file gui-text-draw.h
/// @brief One line of text to draw, and how.
/// @par Threading
/// Immutable value type (views its text).

#include "draw-pos.h"
#include "gui-color.h"
#include "gui-font.h"

#include <string_view>

namespace eng {

/// A line of text for `GuiDrawContext::drawText`.
struct GuiTextDraw {
  /// UTF-8 text; not owned.
  std::string_view text{};
  /// Top-left of the line's box; the baseline is an ascender below, plus
  /// half the extra leading a taller `line_height` adds.
  DrawPos pos{};
  /// Its colour.
  GuiColor color{};
  /// How it is set.
  GuiFont font{};
};

}  // namespace eng
