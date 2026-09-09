#pragma once

/// @file editor-action-ops.h
/// @brief Perform, undo, and redo editor actions.
/// @par Threading Main-thread-only.

#include <editor/shell/editor-action-history.h>
#include <editor/shell/editor-document.h>
#include <editor/shell/editor-selection.h>

namespace eng::editor {

/// Carry out @p action against @p document and record it as the newest.
///
/// Anything previously undone is dropped first: the redo tail described a
/// document this edit has just diverged from, and reapplying it later would
/// splice in operations that no longer make sense.
void performEditorAction(EditorActionHistory& history, EditorDocument& document,
                         const EditorAction& action);

/// Whether an applied action is waiting to be reverted.
[[nodiscard]] bool canUndoEditorAction(const EditorActionHistory& history);

/// Whether a reverted action is waiting to be reapplied.
[[nodiscard]] bool canRedoEditorAction(const EditorActionHistory& history);

/// Revert the newest applied action. False when there is none, which is the
/// caller's cue that nothing about the document changed.
bool undoEditorAction(EditorActionHistory& history, EditorDocument& document);

/// Reapply the oldest reverted action. False when there is none.
bool redoEditorAction(EditorActionHistory& history, EditorDocument& document);

/// Where the selection lands after @p action is undone.
///
/// Selection is not itself undoable — nobody expects Ctrl+Z to give them
/// back a highlight — but it names an entry by index, and an undo that
/// removes or reinserts one renumbers the list under it. Left alone, the
/// panel would show the properties of whatever slid into that slot. A
/// selection in the other list is untouched: the two are numbered
/// separately, so an edit to one says nothing about the other.
[[nodiscard]] EditorSelection
editorSelectionAfterUndo(const EditorAction& action, EditorSelection selection);

/// Where the selection lands after @p action is redone.
[[nodiscard]] EditorSelection
editorSelectionAfterRedo(const EditorAction& action, EditorSelection selection);

/// Whether the document has changed since it was last written to disk.
[[nodiscard]] bool hasUnsavedEditorChanges(const EditorActionHistory& history);

/// Record that the document now matches what is on disk. What a successful
/// save calls, and what opening a project calls once its level is read.
void markEditorChangesSaved(EditorActionHistory& history);

/// Record that the document no longer matches what is on disk, for a change
/// that is not an action: a rescan dropping a prop whose asset has gone.
void markEditorChangesUnsaved(EditorActionHistory& history);

/// Forget every action, applied or not.
///
/// The history describes one document by index, so whatever replaces that
/// document — a rescan renumbering the assets, a project being closed —
/// invalidates all of it. Keeping actions across that would let undo write
/// old indices into a new list.
///
/// Leaves the unsaved marker alone: forgetting how the document got here
/// says nothing about whether it matches the file, and only the caller
/// replacing the document knows which it is.
void clearEditorActions(EditorActionHistory& history);

}  // namespace eng::editor
