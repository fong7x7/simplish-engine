#pragma once

/// @file gui-popover-placement.h
/// @brief Where a popover goes next to its anchor, kept on screen.
/// @par Threading
/// Pure.

#include "gui-popover-side.h"
#include "gui-rect.h"
#include "layout-engine.h"
#include "layout-size.h"

namespace eng {

/// How a popover — a menu, a tooltip, a context menu, a picker — sits
/// against the rect it hangs from.
struct GuiPopoverPlacement {
  /// The side it prefers.
  GuiPopoverSide side = GuiPopoverSide::BELOW;
  /// Along that side: START lines up the leading edges, CENTER centres,
  /// END lines up the trailing edges.
  Align align = Align::START;
  /// Space between it and the anchor.
  float gap = 4.0f;
  /// Space kept between it and the viewport's edges.
  float margin = 8.0f;
};

/// Where a popover @p size big goes against @p anchor inside @p viewport:
/// on @p placement's side unless it does not fit there and the opposite
/// side has more room, then slid along both axes to stay inside the
/// viewport less its margin. What every popover in the GUI is placed by.
[[nodiscard]] Rect placePopover(const Rect& anchor, LayoutSize size,
                                const Rect& viewport,
                                const GuiPopoverPlacement& placement);

}  // namespace eng
