#pragma once

/// @file editor-ground-change.h
/// @brief One cell a ground edit repainted.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>
#include <engine/render-ground/ground-cell.h>

namespace eng::editor {

/// A cell a paint stroke changed, with the terrain on both sides of the
/// change: an edit to the ground is the list of these, and undoing it is
/// writing each `before` back.
/// @thread_safety Immutable value type.
struct EditorGroundChange {
  /// Which cell.
  GroundCell cell{};
  /// The terrain it held before.
  uint8_t before = 0;
  /// The terrain it holds after.
  uint8_t after = 0;

  /// Two changes are the same when they repaint the same cell the same way.
  bool operator==(const EditorGroundChange&) const = default;
};

}  // namespace eng::editor
