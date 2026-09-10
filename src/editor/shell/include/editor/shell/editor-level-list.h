#pragma once

/// @file editor-level-list.h
/// @brief The levels a project holds, read off the disk.
/// @par Threading Main-thread-only (touches the filesystem).

#include <editor/shell/editor-level-entry.h>
#include <editor/shell/editor-shell-state.h>
#include <string_view>
#include <vector>

namespace eng::editor {

/// Every level of @p state's project, in id order, the open one included
/// whether or not it has a file yet.
///
/// Sorted rather than left in the order the filesystem enumerated them:
/// directory order is neither stable across machines nor meaningful to
/// anybody, and a menu whose rows move between sessions is a menu nobody
/// can learn ([design principle
/// 1](../../../../../docs/development/design-principles.md)). Empty with no
/// project open.
[[nodiscard]] std::vector<EditorLevelEntry>
scanEditorLevels(const EditorShellState& state);

/// Re-read that list into @p state, which is where the menu and the agent
/// API both read it from.
void refreshEditorLevels(EditorShellState& state);

/// Whether @p state's project holds a level called @p id — on disk, or as
/// the open level of a project nothing has been saved into yet.
[[nodiscard]] bool hasEditorLevel(const EditorShellState& state,
                                  std::string_view id);

}  // namespace eng::editor
