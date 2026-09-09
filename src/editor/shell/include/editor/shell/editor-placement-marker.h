#pragma once

/// @file editor-placement-marker.h
/// @brief What the viewport draws and picks for one placement.
/// @par Threading Main-thread-only.

#include <editor/shell/editor-placement-bounds.h>

namespace eng::editor {

/// One thing in the level, as much of it as the viewport needs.
///
/// The viewport knows nothing about assets, meshes, lights, or which list
/// an entry came from — it is handed a box and told whether it is the
/// selected one. That keeps the widget testable without a project, keeps
/// the editor the single place that decides what is selected, and is what
/// let lights reuse the overlay and the picking a placement already had.
/// @thread_safety Main-thread-only.
struct EditorPlacementMarker {
  /// World box the placement occupies.
  PlacementBounds bounds{};
  /// Whether this is the selected placement.
  bool selected = false;
};

}  // namespace eng::editor
