#pragma once

/// @file editor-nav-overlay.h
/// @brief What the viewport draws of the level's navigation grid.
/// @par Threading Main-thread-only.

#include <editor/shell/editor-nav-run.h>
#include <editor/shell/editor-navigation.h>
#include <engine/math/vec2.h>
#include <vector>

namespace eng::editor {

/// The navigation overlay: every cell an actor cannot use, as runs along
/// each row, placed on the floor (Editor REQUIREMENTS §4.3).
///
/// Open cells are not drawn at all, so the overlay says only what is wrong
/// with the floor: where props are, where it is too narrow to stand, and
/// where no player start can be reached from.
/// @thread_safety Main-thread-only.
struct EditorNavOverlay {
  /// World X and Y of the corner of cell (0, 0).
  Vec2 origin{};
  /// The side of one cell, in tiles.
  float cell_size = 0.0F;
  /// World Z of the floor the grid lies on.
  float floor_z = 0.0F;
  /// Every run of cells that are not open, row by row.
  std::vector<EditorNavRun> runs{};
};

/// The overlay for @p navigation.
[[nodiscard]] EditorNavOverlay
editorNavOverlay(const EditorNavigation& navigation);

}  // namespace eng::editor
