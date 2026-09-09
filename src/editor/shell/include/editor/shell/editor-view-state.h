#pragma once

/// @file editor-view-state.h
/// @brief What the viewport is showing, mirrored out of the widget.
/// @par Threading Main-thread-only.

#include <editor/shell/iso-camera.h>
#include <editor/shell/iso-projection.h>

namespace eng::editor {

/// Where the viewport camera sits, and what it is over.
///
/// The widget owns these — this is a copy the editor refreshes each tick.
/// It exists so that everything the editor knows can be read from
/// `EditorShellState` alone, which is what lets the agent API
/// (`src/editor/agent/`) answer questions about the camera without holding
/// a widget, and lets a test assert on the view with no GUI tree at all.
/// Nothing reads back the other way: writing here would not move the
/// camera, so the camera commands go through the menu command that already
/// moves it.
/// @thread_safety Main-thread-only.
struct EditorViewState {
  /// The viewport camera's focus and zoom, as of the last tick.
  IsoCamera camera{};
  /// The tile under the pointer. Meaningless while `hovered` is false.
  WorldPoint hovered_tile{};
  /// Whether the pointer is over the viewport at all.
  bool hovered = false;
  /// Whether the tile grid is being drawn.
  bool show_grid = true;
};

}  // namespace eng::editor
