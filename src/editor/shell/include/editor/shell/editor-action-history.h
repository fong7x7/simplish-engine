#pragma once

/// @file editor-action-history.h
/// @brief The session's undo and redo stacks, as one list and a cursor.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <editor/shell/editor-action.h>
#include <vector>

namespace eng::editor {

/// Every action taken this session, and how far through them the document
/// currently is.
///
/// One list rather than two stacks: `[0, applied)` are the actions in
/// effect, and `[applied, actions.size())` are the ones undone and waiting
/// to be redone. Undo and redo then move the cursor without moving any
/// action between containers, and recording a new edit after an undo is a
/// truncation rather than a stack to discard.
///
/// The list is unbounded within a session, as [Editor REQUIREMENTS §3]
/// asks, and nothing persists it — it dies with the editor, as the
/// placements it describes already do.
/// @thread_safety Main-thread-only.
struct EditorActionHistory {
  /// Actions in the order they were first applied, oldest first.
  std::vector<EditorAction> actions;
  /// How many leading entries of `actions` are currently applied.
  size_t applied = 0;
  /// Whether the document has changed since it was last written to disk.
  ///
  /// It lives here, rather than in `EditorShellState` beside the project,
  /// because this is the one record every change to the document passes
  /// through: `performEditorAction`, `undoEditorAction` and
  /// `redoEditorAction` are what the panels *and* the agent API both call,
  /// so one line in each of them is the whole of the bookkeeping. A flag
  /// on the shell would have to be set at every call site instead, and the
  /// one somebody forgot would be a level that says it is saved and is
  /// not.
  ///
  /// Not derived from `applied`: undoing back to where the last save left
  /// the cursor and then making a different edit lands on the same number
  /// with a different document.
  bool unsaved_changes = false;
};

}  // namespace eng::editor
