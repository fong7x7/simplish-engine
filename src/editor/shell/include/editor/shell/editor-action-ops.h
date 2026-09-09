#pragma once

/// @file editor-action-ops.h
/// @brief Perform, undo, and redo editor actions.
/// @par Threading Main-thread-only.

#include <editor/shell/editor-action-history.h>
#include <editor/shell/editor-placement.h>
#include <editor/shell/editor-selection.h>
#include <vector>

namespace eng::editor {

/// Carry out @p action against @p placements and record it as the newest.
///
/// Anything previously undone is dropped first: the redo tail described a
/// document this edit has just diverged from, and reapplying it later would
/// splice in operations that no longer make sense.
void performEditorAction(EditorActionHistory& history,
                         std::vector<EditorPlacement>& placements,
                         const EditorAction& action);

/// Whether an applied action is waiting to be reverted.
[[nodiscard]] bool canUndoEditorAction(const EditorActionHistory& history);

/// Whether a reverted action is waiting to be reapplied.
[[nodiscard]] bool canRedoEditorAction(const EditorActionHistory& history);

/// Revert the newest applied action. False when there is none, which is the
/// caller's cue that nothing about the document changed.
bool undoEditorAction(EditorActionHistory& history,
                      std::vector<EditorPlacement>& placements);

/// Reapply the oldest reverted action. False when there is none.
bool redoEditorAction(EditorActionHistory& history,
                      std::vector<EditorPlacement>& placements);

/// Where the selection lands after @p action is undone.
///
/// Selection is not itself undoable — nobody expects Ctrl+Z to give them
/// back a highlight — but it names a placement by index, and an undo that
/// removes or reinserts one renumbers the list under it. Left alone, the
/// panel would show the properties of whatever slid into that slot.
[[nodiscard]] int editorSelectionAfterUndo(const EditorAction& action,
                                           int selection);

/// Where the selection lands after @p action is redone.
[[nodiscard]] int editorSelectionAfterRedo(const EditorAction& action,
                                           int selection);

/// Forget every action, applied or not.
///
/// The history describes one document by index, so whatever replaces that
/// document — a rescan renumbering the assets, a project being closed —
/// invalidates all of it. Keeping actions across that would let undo write
/// old indices into a new list.
void clearEditorActions(EditorActionHistory& history);

}  // namespace eng::editor
