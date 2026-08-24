#pragma once

/// @file dock-region.h
/// @brief One region in a dockspace: which edge, how much it reserves, and
/// which widget is assigned to it.
/// See docs/engine/gui/dockspace.md.

#include "dock-edge.h"
#include "gui-widget-id.h"

namespace eng {

/// One region entry in a `GuiDockLayout`. Describes a single docked slot:
/// which edge it occupies, how many pixels it reserves, and the id of the
/// widget assigned to it (if any).
///
/// Reserved size semantics:
///   - TOP / BOTTOM: `size_px` is the reserved height; width always spans
///     the full dockspace width.
///   - LEFT / RIGHT: `size_px` is the reserved width; height spans the
///     remainder between TOP and BOTTOM regions (corner precedence — see
///     ADR-013).
///   - CENTRE: `size_px` is ignored; the region consumes the remainder.
///
/// An empty region has `child == GUI_WIDGET_ID_INVALID` and contributes
/// zero budget regardless of `size_px` (Q5 resolution).
/// @thread_safety Immutable value type (set once at layout load).
struct DockRegion {
  /// Which edge this region occupies.
  DockEdge edge = DockEdge::CENTRE;
  /// Reserved size in pixels (height for TOP/BOTTOM, width for LEFT/RIGHT,
  /// ignored for CENTRE). Must be >= 0.
  float size_px = 0.0f;
  /// Widget id of the child assigned to this region, or
  /// `GUI_WIDGET_ID_INVALID` if the region is empty.
  GuiWidgetId child = GUI_WIDGET_ID_INVALID;
};

}  // namespace eng
