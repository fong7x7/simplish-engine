#pragma once

/// @file editor-action-history.h
/// @brief The session's undo and redo stacks, as one list and a cursor.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <editor/shell/editor-action.h>
#include <vector>

namespace eng::editor {

/// No position of the undo cursor means "this matches the file".
///
/// What `EditorActionHistory::saved_at` holds once the saved point falls
/// in a redo tail that a new edit discards: the document can no longer be
/// undone back to what was written, so nothing short of another save makes
/// the two agree. A sentinel rather than an optional because this is only
/// ever compared against `applied`, and a value that can never equal one
/// says precisely that.
inline constexpr size_t EDITOR_SAVED_POINT_NONE = static_cast<size_t>(-1);

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
  /// What `applied` was when the document was last written to disk, or
  /// `EDITOR_SAVED_POINT_NONE` when no position means that any more.
  ///
  /// The document matches its file exactly when `applied` equals this, so
  /// undoing every edit made since a save leaves the two agreeing again
  /// and the unsaved marker comes off. A plain "has changed" flag could
  /// not say that: it only ever goes one way, and would leave a document
  /// identical to what is on disk claiming otherwise.
  ///
  /// It lives here, rather than in `EditorShellState` beside the project,
  /// because this is the one record every change to the document passes
  /// through: `performEditorAction`, `undoEditorAction` and
  /// `redoEditorAction` are what the panels *and* the agent API both call.
  /// A flag on the shell would have to be maintained at every call site
  /// instead, and the one somebody forgot would be a level that says it is
  /// saved and is not.
  size_t saved_at = 0;
};

}  // namespace eng::editor
