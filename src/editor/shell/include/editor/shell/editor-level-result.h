#pragma once

/// @file editor-level-result.h
/// @brief What creating or opening a level did, and what it cost.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <editor/shell/editor-level-status.h>

namespace eng::editor {

/// The outcome of a level operation, and the props it could not keep.
///
/// The count travels with the status for the reason `EditorLevelLoad`
/// carries one: a level referencing an asset the project no longer holds is
/// a real thing that happens, and whoever asked for the level — a person at
/// the menu or an agent at the API — is owed the number rather than having
/// it appear only in a log nobody is reading.
/// @thread_safety Main-thread-only.
struct EditorLevelResult {
  /// What the operation did, or why it did nothing.
  EditorLevelStatus status = EditorLevelStatus::OK;
  /// How many props the level lost because nothing in the project answers
  /// to the asset they name. Zero for the ordinary case.
  size_t dropped_props = 0;
};

}  // namespace eng::editor
