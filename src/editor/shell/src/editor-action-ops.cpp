#include <cstddef>
#include <editor/shell/editor-action-ops.h>
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

  /// Do what @p action describes.
  void applyOne(const EditorAction& action,
                std::vector<EditorPlacement>& placements) {
    switch (action.kind) {
      case EditorActionKind::PLACE_ASSET:
        insertPlacement(action, placements);
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
    }
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

void clearEditorActions(EditorActionHistory& history) {
  history.actions.clear();
  history.applied = 0;
}

}  // namespace eng::editor
