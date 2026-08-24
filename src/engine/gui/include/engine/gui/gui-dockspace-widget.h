#pragma once

/// @file gui-dockspace-widget.h
/// @brief Widget that subdivides its own rect into named dock regions and
/// assigns each region's rect to a docked child. The generic region
/// allocator that replaces per-panel manual rect math — consumed by both
/// the editor shell and the game HUD.
///
/// See docs/engine/gui/dockspace.md (requirements),
/// docs/technical-approaches/engine/gui/dockspace.md (Phase 1 leaf), and
/// docs/decisions/ADR-013-dockspace-owns-panel-regions.md.

#include "dock-edge.h"
#include "gui-dock-layout.h"
#include "gui-draw-context.h"
#include "gui-rect.h"
#include "gui-widget-id.h"
#include "gui-widget.h"

#include <array>
#include <memory>
#include <optional>
#include <unordered_set>

namespace eng {

/// Widget that owns region allocation for its docked children. Overrides
/// `arrangeChildren` to compute dock-region rects from a `GuiDockLayout`
/// and assign each rect to the corresponding child via the widget tree's
/// arrange pass. Does not paint — children render themselves via normal
/// tree draw order.
///
/// Typical usage: insert a `GuiDockspaceWidget` as the widget tree's
/// root, insert panels as children of the dockspace, then call
/// `assignChild` to map each panel id to a `DockEdge`. Resize the window
/// — the dockspace recomputes every child's rect during the next layout
/// pass.
/// @thread_safety Main thread only.
class GuiDockspaceWidget : public GuiWidget {
public:
  /// Construct with an initial layout. The layout's child ids may be
  /// `GUI_WIDGET_ID_INVALID` (regions filled later via `assignChild`).
  explicit GuiDockspaceWidget(GuiDockLayout layout);

  /// Assign `child` to the given `edge`. Replaces any prior assignment on
  /// that edge. Returns `false` and logs if `child` is already assigned
  /// to a different region in this dockspace (no-op on failure). Passing
  /// `GUI_WIDGET_ID_INVALID` clears the region.
  bool assignChild(DockEdge edge, GuiWidgetId child);

  /// Clear the child assignment for `edge`. The widget tree node is not
  /// destroyed; it simply stops being arranged by this dockspace.
  void clearRegion(DockEdge edge);

  /// Most recently arranged rect for `edge`, or `std::nullopt` if the
  /// region is empty or no arrange pass has run yet.
  std::optional<Rect> regionRect(DockEdge edge) const;

  /// Replace the layout config wholesale (e.g. after a JSON hot-reload).
  /// The new layout takes effect on the next `GuiWidgetTree::computeLayout`
  /// pass — `computeLayout` always arranges, so no explicit dirty mark is
  /// required. `regionRect` returns `std::nullopt` until that next arrange.
  void setLayout(GuiDockLayout layout);

  /// Read-only access to the current layout. Exposed for tests and debug
  /// overlays; production code should prefer `regionRect`.
  const GuiDockLayout& layout() const;

  // GuiWidget overrides -------------------------------------------------

  std::unique_ptr<GuiWidget> clone() const override;
  void render(const GuiDrawContext& ctx) const override;
  void arrangeChildren(GuiWidgetTree& tree, const Rect& available) override;

private:
  /// Current layout config. Replaced wholesale by `setLayout`.
  GuiDockLayout layout_{};
  /// Cached region rects populated by `arrangeChildren`. Indexed by
  /// `static_cast<size_t>(DockEdge)`.
  std::array<Rect, DOCK_EDGE_COUNT> region_rects_{};
  /// True once `arrangeChildren` has run at least once against this
  /// layout. Reset by `setLayout`.
  bool has_layout_ = false;

  /// Widget ids that have already been logged as unassigned; prevents
  /// per-frame log spam from a persistently mis-attached child. Cleared
  /// by `setLayout` since layout replacement is the natural point to
  /// reconsider assignments.
  std::unordered_set<GuiWidgetId> warned_unassigned_{};

  /// Debug-only post-arrange check that no two occupied regions intersect
  /// (invariant I2 from the Phase 1 leaf). No-op in release builds.
  void assertNoOverlaps() const;

  /// Post-arrange diagnostic: emits a `Logger::warn` once per unique
  /// child id that is a tree child of this dockspace but not referenced
  /// by any region in `layout_`. Catches the "attached via
  /// `insertExternalWidget` but never `assignChild`" footgun (M1 finding
  /// in Phase 4 review). Fires in release and debug builds; dedups
  /// against `warned_unassigned_` so a persistently mis-attached child
  /// logs at most once per dockspace lifetime (or per `setLayout`).
  void warnUnassignedChildren();
};

}  // namespace eng
