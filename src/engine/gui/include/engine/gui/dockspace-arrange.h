#pragma once

/// @file dockspace-arrange.h
/// @brief Pure-function region arithmetic for `GuiDockspaceWidget`. Kept as
/// free functions so tests can exercise the layout math without building
/// a widget tree.
/// See docs/technical-approaches/engine/gui/dockspace.md §2.2.

#include "dock-edge.h"
#include "gui-dock-layout.h"
#include "gui-rect.h"

#include <array>

namespace eng {

/// Compute the rect for every dock region inside `viewport` given the
/// reserved sizes in `layout`. Writes one rect per `DockEdge` into
/// `output`, indexed by `static_cast<size_t>(edge)`.
///
/// Edge precedence (pinned by ADR-013):
///   - TOP and BOTTOM span the full viewport width.
///   - LEFT and RIGHT span the remaining height between TOP and BOTTOM.
///   - CENTRE is the interior remainder.
///
/// Empty regions (`child == GUI_WIDGET_ID_INVALID`) contribute zero budget
/// regardless of their `size_px` (Q5 resolution).
///
/// Returns `true` when the centre region has positive area; `false` when
/// the viewport is too small to satisfy all reserved sizes (centre
/// clamped to zero). Callers decide whether to assert or log.
/// @thread_safety Thread-safe (pure function).
bool computeDockRegions(const GuiDockLayout& layout, const Rect& viewport,
                        std::array<Rect, DOCK_EDGE_COUNT>& output);

/// True iff rects `a` and `b` have a positive-area intersection. Used by
/// `GuiDockspaceWidget` to enforce the no-overlap invariant in debug builds
/// and by tests to validate composite layouts.
/// @thread_safety Thread-safe (pure function).
bool rectsOverlap(const Rect& a, const Rect& b);

}  // namespace eng
