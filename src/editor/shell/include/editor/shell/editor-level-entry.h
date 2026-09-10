#pragma once

/// @file editor-level-entry.h
/// @brief One level of the open project, as the editor lists it.
/// @par Threading Main-thread-only.

#include <string>

namespace eng::editor {

/// A level the open project holds, or is about to.
///
/// The id is the whole identity: a level file is `<id>.level.json` and a
/// scenario names a level by `level:<id>` ([project-format.md §3]), so
/// nothing else here has to be stored to find it again. A display name is
/// absent because nothing in the editor authors one yet — the file's `name`
/// is the id until something does.
/// @thread_safety Main-thread-only.
struct EditorLevelEntry {
  /// Level id: its file's name with `.level.json` taken off.
  std::string id;
  /// Whether a file for it is on disk. False for the level a project that
  /// has never been saved is open at, which is an ordinary state.
  bool on_disk = false;
};

}  // namespace eng::editor
