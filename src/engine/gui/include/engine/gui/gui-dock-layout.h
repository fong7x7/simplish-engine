#pragma once

/// @file gui-dock-layout.h
/// @brief Layout configuration for one `GuiDockspaceWidget` instance.
/// See docs/engine/gui/dockspace.md and
/// docs/technical-approaches/engine/gui/dockspace.md §2.1.

#include "dock-edge.h"
#include "dock-region.h"

#include <array>
#include <vector>

namespace eng {

/// Layout configuration for a single dockspace. Loaded from
/// `data/config/editor/dock-layout.json` (editor) or
/// `data/config/game/hud-layout.json` (game HUD). Immutable after
/// construction; to change, replace the dockspace's layout and mark dirty.
/// @thread_safety Main thread only (constructed and consumed from main).
struct GuiDockLayout {
  /// Fixed region slots indexed by `static_cast<size_t>(DockEdge)`. One
  /// entry per edge plus centre. Empty regions have
  /// `child == GUI_WIDGET_ID_INVALID`.
  std::array<DockRegion, DOCK_EDGE_COUNT> regions{};
  /// Reserved for M9 split/tab extensions. Must be empty in M1 (loader
  /// asserts and rejects non-empty values at load time).
  std::vector<DockRegion> extra_regions{};
};

}  // namespace eng
