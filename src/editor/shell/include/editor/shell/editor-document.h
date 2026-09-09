#pragma once

/// @file editor-document.h
/// @brief Everything the editor has authored into the level so far.
/// @par Threading Main-thread-only.

#include <editor/shell/editor-light.h>
#include <editor/shell/editor-placement.h>
#include <vector>

namespace eng::editor {

/// The level as the editor holds it: what has been placed, and what lights
/// it.
///
/// One record rather than a list per kind passed around separately, because
/// the history describes all of it: an action names a list and a slot in
/// it, and undo has to reach whichever list that was. This is what a level
/// file holds: `editor-level-json.h` writes it and reads it back.
/// @thread_safety Main-thread-only.
struct EditorDocument {
  /// Assets placed in the world, in the order they were placed.
  std::vector<EditorPlacement> placements;
  /// Lights placed in the world, in the order they were added.
  std::vector<EditorLight> lights;
};

}  // namespace eng::editor
