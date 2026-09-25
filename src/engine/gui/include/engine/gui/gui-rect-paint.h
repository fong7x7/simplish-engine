#pragma once

/// @file gui-rect-paint.h
/// @brief Everything `GuiDrawContext::drawRect` paints a rect with.
/// @par Threading
/// Immutable value type.

#include "gui-color.h"
#include "gui-corners.h"
#include "gui-gradient.h"
#include "gui-rect.h"
#include "layout-edges.h"

#include <optional>

namespace eng {

/// A rect as CSS would paint a box: a fill or a gradient, each corner's
/// radius, and a border whose width may differ per side.
struct GuiRectPaint {
  /// Where, in layout pixels.
  Rect rect{};
  /// The fill; zero alpha for none. Ignored when `gradient` is set.
  GuiColor fill{0, 0, 0, 0};
  /// A gradient in place of the fill.
  std::optional<GuiGradient> gradient{};
  /// Corner radii.
  GuiCorners radii{};
  /// Border width per side; all zero for none.
  Edges border{};
  /// Border colour.
  GuiColor border_color{0, 0, 0, 0};
};

}  // namespace eng
