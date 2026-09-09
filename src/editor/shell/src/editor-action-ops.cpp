#include <cstddef>
#include <editor/shell/editor-action-ops.h>
#include <iterator>
#include <vector>

namespace eng::editor {

namespace {

  /// Put a value back at the slot it was taken from. An index past the end
  /// means the history and the document have diverged, which is a bug
  /// elsewhere — the insert is dropped rather than moving the entry.
  template <typename T>
  void insertAt(std::vector<T>& list, size_t index, const T& value) {
    if (index > list.size()) {
      return;
    }
    list.insert(std::next(list.begin(), static_cast<ptrdiff_t>(index)), value);
  }

  /// Take the entry at @p index back out, if it is still there.
  template <typename T> void eraseAt(std::vector<T>& list, size_t index) {
    if (index >= list.size()) {
      return;
    }
    list.erase(std::next(list.begin(), static_cast<ptrdiff_t>(index)));
  }

  /// Write the entry an action names, if it is still there. A missing index
  /// drops the write rather than growing the list.
  template <typename T>
  void writeAt(std::vector<T>& list, size_t index, const T& value) {
    if (index >= list.size()) {
      return;
    }
    list[index] = value;
  }

  /// Do what @p action describes.
  void applyOne(const EditorAction& action, EditorDocument& document) {
    switch (action.kind) {
      case EditorActionKind::PLACE_ASSET:
        insertAt(document.placements, action.index, action.placement);
        break;
      case EditorActionKind::TRANSFORM_PLACEMENT:
        writeAt(document.placements, action.index, action.placement);
        break;
      case EditorActionKind::ADD_LIGHT:
        insertAt(document.lights, action.index, action.light);
        break;
      case EditorActionKind::TRANSFORM_LIGHT:
        writeAt(document.lights, action.index, action.light);
        break;
    }
  }

  /// Undo what @p action describes. Every case here is the inverse of its
  /// counterpart in `applyOne`, which is the whole contract of an action.
  void revertOne(const EditorAction& action, EditorDocument& document) {
    switch (action.kind) {
      case EditorActionKind::PLACE_ASSET:
        eraseAt(document.placements, action.index);
        break;
      case EditorActionKind::TRANSFORM_PLACEMENT:
        writeAt(document.placements, action.index, action.prior);
        break;
      case EditorActionKind::ADD_LIGHT:
        eraseAt(document.lights, action.index);
        break;
      case EditorActionKind::TRANSFORM_LIGHT:
        writeAt(document.lights, action.index, action.light_prior);
        break;
    }
  }

  /// Where a selection lands once the entry at @p index of @p list is
  /// removed. A selection in another list is not numbered by this one.
  EditorSelection selectionAfterErase(EditorSelectionKind list, size_t index,
                                      EditorSelection selection) {
    if (!selectionIs(selection, list)) {
      return selection;
    }
    if (selection.index == index) {
      return {};
    }
    return {list,
            selection.index > index ? selection.index - 1 : selection.index};
  }

  /// Where a selection lands once an entry is inserted at @p index.
  EditorSelection selectionAfterInsert(EditorSelectionKind list, size_t index,
                                       EditorSelection selection) {
    if (!selectionIs(selection, list)) {
      return selection;
    }
    return {list,
            selection.index >= index ? selection.index + 1 : selection.index};
  }

}  // namespace

void performEditorAction(EditorActionHistory& history, EditorDocument& document,
                         const EditorAction& action) {
  applyOne(action, document);
  history.actions.resize(history.applied);
  history.actions.push_back(action);
  history.applied = history.actions.size();
  history.unsaved_changes = true;
}

bool hasUnsavedEditorChanges(const EditorActionHistory& history) {
  return history.unsaved_changes;
}

void markEditorChangesSaved(EditorActionHistory& history) {
  history.unsaved_changes = false;
}

void markEditorChangesUnsaved(EditorActionHistory& history) {
  history.unsaved_changes = true;
}

bool canUndoEditorAction(const EditorActionHistory& history) {
  return history.applied > 0;
}

bool canRedoEditorAction(const EditorActionHistory& history) {
  return history.applied < history.actions.size();
}

bool undoEditorAction(EditorActionHistory& history, EditorDocument& document) {
  if (!canUndoEditorAction(history)) {
    return false;
  }
  --history.applied;
  revertOne(history.actions[history.applied], document);
  // An undo changes the document as surely as the edit it reverts did.
  // Landing back on exactly what is on disk is possible and is reported as
  // unsaved anyway: the cost of that is one redundant save, and the cost of
  // the other mistake is a lost level.
  history.unsaved_changes = true;
  return true;
}

bool redoEditorAction(EditorActionHistory& history, EditorDocument& document) {
  if (!canRedoEditorAction(history)) {
    return false;
  }
  applyOne(history.actions[history.applied], document);
  ++history.applied;
  history.unsaved_changes = true;
  return true;
}

EditorSelection editorSelectionAfterUndo(const EditorAction& action,
                                         EditorSelection selection) {
  switch (action.kind) {
    case EditorActionKind::PLACE_ASSET:
      // The placement is gone; anything numbered after it moved down one.
      return selectionAfterErase(EditorSelectionKind::PLACEMENT, action.index,
                                 selection);
    case EditorActionKind::ADD_LIGHT:
      return selectionAfterErase(EditorSelectionKind::LIGHT, action.index,
                                 selection);
    case EditorActionKind::TRANSFORM_PLACEMENT:
      // Show what just moved back, so a reverted edit is visible rather
      // than something the viewer has to go looking for.
      return {EditorSelectionKind::PLACEMENT, action.index};
    case EditorActionKind::TRANSFORM_LIGHT:
      return {EditorSelectionKind::LIGHT, action.index};
  }
  return selection;
}

EditorSelection editorSelectionAfterRedo(const EditorAction& action,
                                         EditorSelection selection) {
  switch (action.kind) {
    case EditorActionKind::PLACE_ASSET:
      return selectionAfterInsert(EditorSelectionKind::PLACEMENT, action.index,
                                  selection);
    case EditorActionKind::ADD_LIGHT:
      return selectionAfterInsert(EditorSelectionKind::LIGHT, action.index,
                                  selection);
    case EditorActionKind::TRANSFORM_PLACEMENT:
      return {EditorSelectionKind::PLACEMENT, action.index};
    case EditorActionKind::TRANSFORM_LIGHT:
      return {EditorSelectionKind::LIGHT, action.index};
  }
  return selection;
}

void clearEditorActions(EditorActionHistory& history) {
  history.actions.clear();
  history.applied = 0;
}

}  // namespace eng::editor
