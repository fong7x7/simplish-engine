#pragma once

/// @file editor-level-load.h
/// @brief A level file read back, and what the read could not keep.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <editor/shell/editor-document.h>

namespace eng::editor {

/// The outcome of reading a level file against a project's assets.
///
/// The count travels with the document rather than being logged where the
/// parse happens, because the parse is a pure function with nowhere to say
/// it: a level referencing an asset the project no longer holds is a real
/// thing that happens — a file deleted between sessions — and whoever opens
/// the project is owed the number.
/// @thread_safety Main-thread-only.
struct EditorLevelLoad {
  /// What the file held, every prop already bound to an asset's index in
  /// the list it was read against.
  EditorDocument document;
  /// How many props were dropped because nothing in that list answered to
  /// the asset they named. Zero for the ordinary case.
  size_t dropped_props = 0;
};

}  // namespace eng::editor
