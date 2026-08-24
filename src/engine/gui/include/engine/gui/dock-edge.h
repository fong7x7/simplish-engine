#pragma once

/// @file dock-edge.h
/// @brief Enum naming which edge of a dockspace a region occupies.
/// See docs/engine/gui/dockspace.md and
/// docs/technical-approaches/engine/gui/dockspace.md.

#include <cstddef>
#include <cstdint>

namespace eng {

/// Which edge of a dockspace a `DockRegion` occupies. `CENTRE` is the
/// interior rect remaining after all edge regions are reserved.
/// @thread_safety Immutable value type.
enum class DockEdge : uint8_t {
  /// Docked against the top edge; reserves height, spans full width.
  TOP,
  /// Docked against the bottom edge; reserves height, spans full width.
  BOTTOM,
  /// Docked against the left edge; reserves width, spans height between
  /// top and bottom regions.
  LEFT,
  /// Docked against the right edge; reserves width, spans height between
  /// top and bottom regions.
  RIGHT,
  /// Interior remainder after all edges are reserved. Only one per layout.
  CENTRE,
};

/// Number of fixed dock regions per layout (TOP/BOTTOM/LEFT/RIGHT/CENTRE).
inline constexpr size_t DOCK_EDGE_COUNT = 5;

// Pin DOCK_EDGE_COUNT to the enum size so enum additions force a compile
// error at every site that indexes `regions[]` before fixing the count.
static_assert(static_cast<size_t>(DockEdge::CENTRE) + 1 == DOCK_EDGE_COUNT,
              "DOCK_EDGE_COUNT must equal the number of DockEdge values");

}  // namespace eng
