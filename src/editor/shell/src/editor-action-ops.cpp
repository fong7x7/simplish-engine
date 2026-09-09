#include <cstddef>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-selection.h>
#include <iterator>

namespace eng::editor {

namespace {

  /// `placements.begin() + index`, as the iterator arithmetic wants it.
  std::vector<EditorPlacement>::iterator
  at(std::vector<EditorPlacement>& placements, size_t index) {
    return std::next(placements.begin(), static_cast<ptrdiff_t>(index));
  }

  /// Put an action's placement back where it was.
  void insertPlacement(const EditorAction& action,
                       std::vector<EditorPlacement>& placements) {
    if (action.index > placements.size()) {
      return;
    }
    placements.insert(at(placements, action.index), action.placement);
  }

  /// Take an action's placement back out.
  void erasePlacement(const EditorAction& action,
                      std::vector<EditorPlacement>& placements) {
    if (action.index >= placements.size()) {
      return;
    }
    placements.erase(at(placements, action.index));
  }

  /// Write a placement an action names, if it is still there. A missing
  /// index means the history and the document have diverged, which is a
  /// bug elsewhere — the write is dropped rather than growing the list.
  void writePlacement(size_t index, const EditorPlacement& placement,
                      std::vector<EditorPlacement>& placements) {
    if (index >= placements.size()) {
      return;
    }
    placements[index] = placement;
  }

  /// Do what @p action describes.
  void applyOne(const EditorAction& action,
                std::vector<EditorPlacement>& placements) {
    switch (action.kind) {
      case EditorActionKind::PLACE_ASSET:
        insertPlacement(action, placements);
        break;
      case EditorActionKind::TRANSFORM_PLACEMENT:
        writePlacement(action.index, action.placement, placements);
        break;
    }
  }

  /// Undo what @p action describes. Every case here is the inverse of its
  /// counterpart in `applyOne`, which is the whole contract of an action.
  void revertOne(const EditorAction& action,
                 std::vector<EditorPlacement>& placements) {
    switch (action.kind) {
      case EditorActionKind::PLACE_ASSET:
        erasePlacement(action, placements);
        break;
      case EditorActionKind::TRANSFORM_PLACEMENT:
        writePlacement(action.index, action.prior, placements);
        break;
    }
  }

  /// Where a selection lands once the entry at @p index is removed.
  int selectionAfterErase(size_t index, int selection) {
    const auto erased = static_cast<int>(index);
    if (selection == erased) {
      return EDITOR_PLACEMENT_NONE;
    }
    return selection > erased ? selection - 1 : selection;
  }

  /// Where a selection lands once an entry is inserted at @p index.
  int selectionAfterInsert(size_t index, int selection) {
    const auto inserted = static_cast<int>(index);
    return selection >= inserted ? selection + 1 : selection;
  }

}  // namespace

void performEditorAction(EditorActionHistory& history,
                         std::vector<EditorPlacement>& placements,
                         const EditorAction& action) {
  applyOne(action, placements);
  history.actions.resize(history.applied);
  history.actions.push_back(action);
  history.applied = history.actions.size();
}

bool canUndoEditorAction(const EditorActionHistory& history) {
  return history.applied > 0;
}

bool canRedoEditorAction(const EditorActionHistory& history) {
  return history.applied < history.actions.size();
}

bool undoEditorAction(EditorActionHistory& history,
                      std::vector<EditorPlacement>& placements) {
  if (!canUndoEditorAction(history)) {
    return false;
  }
  --history.applied;
  revertOne(history.actions[history.applied], placements);
  return true;
}

bool redoEditorAction(EditorActionHistory& history,
                      std::vector<EditorPlacement>& placements) {
  if (!canRedoEditorAction(history)) {
    return false;
  }
  applyOne(history.actions[history.applied], placements);
  ++history.applied;
  return true;
}

int editorSelectionAfterUndo(const EditorAction& action, int selection) {
  switch (action.kind) {
    case EditorActionKind::PLACE_ASSET:
      // The placement is gone; anything numbered after it moved down one.
      return selectionAfterErase(action.index, selection);
    case EditorActionKind::TRANSFORM_PLACEMENT:
      // Show what just moved back, so a reverted edit is visible rather
      // than something the viewer has to go looking for.
      return static_cast<int>(action.index);
  }
  return selection;
}

int editorSelectionAfterRedo(const EditorAction& action, int selection) {
  switch (action.kind) {
    case EditorActionKind::PLACE_ASSET:
      return selectionAfterInsert(action.index, selection);
    case EditorActionKind::TRANSFORM_PLACEMENT:
      return static_cast<int>(action.index);
  }
  return selection;
}

void clearEditorActions(EditorActionHistory& history) {
  history.actions.clear();
  history.applied = 0;
}

}  // namespace eng::editor
