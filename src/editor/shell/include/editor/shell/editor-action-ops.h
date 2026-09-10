#pragma once

/// @file editor-action-ops.h
/// @brief Perform, undo, and redo editor actions.
/// @par Threading Main-thread-only.

#include <editor/shell/editor-action-history.h>
#include <editor/shell/editor-document.h>
#include <editor/shell/editor-selection.h>
#include <optional>

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

/// How many entries the document list @p kind names holds, and none for
/// `NONE` — so `selection.index < editorListSize(document, selection.kind)`
/// is the one test for "the selection names something that is there".
[[nodiscard]] size_t editorListSize(const EditorDocument& document,
                                    EditorSelectionKind kind);

/// The action that removes whatever @p selection names, or nothing when
/// it names nothing that is there.
///
/// The action rather than the removal: every route into the document —
/// the Delete key, the Edit menu, the agent's `delete` tool — records the
/// same one, so there is a single description of what a removal is and a
/// single inverse of it. The entry is copied into the action on the way
/// out, which is what lets undo put back the one that was there rather
/// than a fresh one at its index.
[[nodiscard]] std::optional<EditorAction>
editorDeleteAction(const EditorDocument& document, EditorSelection selection);

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
///
/// False again once every edit made since the last save has been undone:
/// the document is then the one in the file, whatever route it took to get
/// back there, and saying otherwise would put an unsaved marker on a level
/// with nothing to save.
[[nodiscard]] bool hasUnsavedEditorChanges(const EditorActionHistory& history);

/// Record that the document now matches what is on disk. What a successful
/// save calls, and what opening a project calls once its level is read.
void markEditorChangesSaved(EditorActionHistory& history);

/// Record that the document no longer matches what is on disk, and that no
/// undo will take it back — for a change that is not an action, such as a
/// rescan dropping a prop whose asset has gone.
void markEditorChangesUnsaved(EditorActionHistory& history);

/// Forget every action, applied or not.
///
/// The history describes one document by index, so whatever replaces that
/// document — a rescan renumbering the assets, a project being closed —
/// invalidates all of it. Keeping actions across that would let undo write
/// old indices into a new list.
///
/// Carries the unsaved marker across rather than the cursor position it
/// was recorded at: forgetting how the document got here says nothing
/// about whether it matches the file, but there is no longer a list of
/// actions for a position to index into.
void clearEditorActions(EditorActionHistory& history);

}  // namespace eng::editor
