#include <array>
#include <cstddef>
#include <cstdint>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-ground-ops.h>
#include <iterator>
#include <optional>
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

  /// What an action does to the one list its kind names.
  ///
  /// Every kind is one of these three edits to one of the document's lists, and
  /// saying that once is what keeps `applyOne` and `revertOne` short as
  /// kinds are added: a new kind answers two questions rather than growing
  /// four switches.
  enum class ListEdit : uint8_t { INSERT, WRITE, ERASE };

  /// Which list an action kind edits, and how.
  struct ActionShape {
    /// The document list it names.
    EditorSelectionKind list = EditorSelectionKind::PLACEMENT;
    /// What it does to that list when applied.
    ListEdit edit = ListEdit::WRITE;
  };

  /// Every kind's shape, in `EditorActionKind` order: a table, so a kind
  /// added to the enum without a row here fails the assertion below rather
  /// than falling through a switch.
  constexpr ActionShape ACTION_SHAPES[] = {
      {EditorSelectionKind::PLACEMENT, ListEdit::INSERT},
      {EditorSelectionKind::PLACEMENT, ListEdit::WRITE},
      {EditorSelectionKind::PLACEMENT, ListEdit::ERASE},
      {EditorSelectionKind::LIGHT, ListEdit::INSERT},
      {EditorSelectionKind::LIGHT, ListEdit::WRITE},
      {EditorSelectionKind::LIGHT, ListEdit::ERASE},
      {EditorSelectionKind::PLAYER_START, ListEdit::INSERT},
      {EditorSelectionKind::PLAYER_START, ListEdit::WRITE},
      {EditorSelectionKind::PLAYER_START, ListEdit::ERASE},
      {EditorSelectionKind::WAYPOINT, ListEdit::INSERT},
      {EditorSelectionKind::WAYPOINT, ListEdit::WRITE},
      {EditorSelectionKind::WAYPOINT, ListEdit::ERASE},
      {EditorSelectionKind::EMITTER, ListEdit::INSERT},
      {EditorSelectionKind::EMITTER, ListEdit::WRITE},
      {EditorSelectionKind::EMITTER, ListEdit::ERASE},
      {EditorSelectionKind::SPRITE, ListEdit::INSERT},
      {EditorSelectionKind::SPRITE, ListEdit::WRITE},
      {EditorSelectionKind::SPRITE, ListEdit::ERASE},
      // The ground is no list and has nothing to select; `applyOne` and
      // `revertOne` take this kind aside before the table is asked.
      {EditorSelectionKind::NONE, ListEdit::WRITE},
  };

  static_assert(std::size(ACTION_SHAPES) ==
                    static_cast<size_t>(EditorActionKind::PAINT_GROUND) + 1,
                "every action kind needs a list and an edit");

  /// Which of the document's lists @p kind names.
  EditorSelectionKind actionList(EditorActionKind kind) {
    return ACTION_SHAPES[static_cast<size_t>(kind)].list;
  }

  /// The edit @p kind makes to that list when it is applied.
  ListEdit appliedEdit(EditorActionKind kind) {
    return ACTION_SHAPES[static_cast<size_t>(kind)].edit;
  }

  /// The edit that undoes @p edit. This is the whole contract of an
  /// action: adding and removing are each other's inverse, and replacing
  /// is its own once the value it replaced is put back.
  ListEdit invertedEdit(ListEdit edit) {
    switch (edit) {
      case ListEdit::INSERT:
        return ListEdit::ERASE;
      case ListEdit::ERASE:
        return ListEdit::INSERT;
      case ListEdit::WRITE:
        return ListEdit::WRITE;
    }
    return ListEdit::WRITE;
  }

  /// Do @p edit to @p list at @p index, with @p value for the two edits
  /// that write one.
  template <typename T>
  void editList(std::vector<T>& list, ListEdit edit, size_t index,
                const T& value) {
    switch (edit) {
      case ListEdit::INSERT:
        insertAt(list, index, value);
        break;
      case ListEdit::WRITE:
        writeAt(list, index, value);
        break;
      case ListEdit::ERASE:
        eraseAt(list, index);
        break;
    }
  }

  /// Which of the two values an action carries an edit writes: the one it
  /// leaves behind, or — for undoing a transform — the one it replaced.
  enum class ActionValue : uint8_t { CURRENT, PRIOR };

  /// The half of an action @p value names: @p current, or @p before.
  template <typename T>
  T pick(ActionValue value, const T& current, const T& before) {
    return value == ActionValue::PRIOR ? before : current;
  }

  /// `editDocument` for the lists the level holds beside its props and
  /// lights: player starts, waypoints, particle emitters and billboards.
  void editMarkerList(const EditorAction& action, ListEdit edit,
                      ActionValue value, EditorDocument& document) {
    const EditorSelectionKind list = actionList(action.kind);
    if (list == EditorSelectionKind::PLAYER_START) {
      editList(document.player_starts, edit, action.index,
               pick(value, action.player_start, action.player_start_prior));
    } else if (list == EditorSelectionKind::WAYPOINT) {
      editList(document.waypoints, edit, action.index,
               pick(value, action.waypoint, action.waypoint_prior));
    } else if (list == EditorSelectionKind::EMITTER) {
      editList(document.emitters, edit, action.index,
               pick(value, action.emitter, action.emitter_prior));
    } else {
      editList(document.sprites, edit, action.index,
               pick(value, action.sprite, action.sprite_prior));
    }
  }

  /// Do @p edit to the list @p action names, writing the value @p value
  /// picks out of the action.
  void editDocument(const EditorAction& action, ListEdit edit,
                    ActionValue value, EditorDocument& document) {
    const EditorSelectionKind list = actionList(action.kind);
    if (list == EditorSelectionKind::PLACEMENT) {
      editList(document.placements, edit, action.index,
               pick(value, action.placement, action.prior));
    } else if (list == EditorSelectionKind::LIGHT) {
      editList(document.lights, edit, action.index,
               pick(value, action.light, action.light_prior));
    } else {
      editMarkerList(action, edit, value, document);
    }
  }

  /// Do what @p action describes.
  void applyOne(const EditorAction& action, EditorDocument& document) {
    if (action.kind == EditorActionKind::PAINT_GROUND) {
      applyEditorGroundChanges(document.ground, action.ground,
                               EditorGroundSide::AFTER);
      return;
    }
    editDocument(action, appliedEdit(action.kind), ActionValue::CURRENT,
                 document);
  }

  /// Undo what @p action describes: the inverse edit, and — for the one
  /// that replaces a value — the value the action found there. Undoing a
  /// removal inserts what the action carried away, so the entry comes back
  /// as it was rather than as a default one wearing its index.
  void revertOne(const EditorAction& action, EditorDocument& document) {
    if (action.kind == EditorActionKind::PAINT_GROUND) {
      applyEditorGroundChanges(document.ground, action.ground,
                               EditorGroundSide::BEFORE);
      return;
    }
    const ListEdit edit = invertedEdit(appliedEdit(action.kind));
    editDocument(action, edit,
                 edit == ListEdit::WRITE ? ActionValue::PRIOR
                                         : ActionValue::CURRENT,
                 document);
  }

  /// The kind of action that takes an entry out of the list @p kind names.
  /// Only asked of a list that exists.
  EditorActionKind removalKind(EditorSelectionKind kind) {
    if (kind == EditorSelectionKind::LIGHT) {
      return EditorActionKind::REMOVE_LIGHT;
    }
    if (kind == EditorSelectionKind::WAYPOINT) {
      return EditorActionKind::REMOVE_WAYPOINT;
    }
    if (kind == EditorSelectionKind::EMITTER) {
      return EditorActionKind::REMOVE_EMITTER;
    }
    if (kind == EditorSelectionKind::SPRITE) {
      return EditorActionKind::REMOVE_SPRITE;
    }
    return kind == EditorSelectionKind::PLAYER_START
               ? EditorActionKind::REMOVE_PLAYER_START
               : EditorActionKind::REMOVE_PLACEMENT;
  }

  /// Copy the entry @p action names out of @p document into the action, so
  /// undoing the removal can put that one back.
  void captureEntry(const EditorDocument& document, EditorAction& action) {
    const EditorSelectionKind list = actionList(action.kind);
    if (list == EditorSelectionKind::PLACEMENT) {
      action.placement = document.placements[action.index];
    } else if (list == EditorSelectionKind::LIGHT) {
      action.light = document.lights[action.index];
    } else if (list == EditorSelectionKind::PLAYER_START) {
      action.player_start = document.player_starts[action.index];
    } else if (list == EditorSelectionKind::WAYPOINT) {
      action.waypoint = document.waypoints[action.index];
    } else if (list == EditorSelectionKind::EMITTER) {
      action.emitter = document.emitters[action.index];
    } else {
      action.sprite = document.sprites[action.index];
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
  // The redo tail goes, and the saved point with it when it sat in there:
  // a document that can no longer be undone back to the one on disk has no
  // cursor position left that means "saved".
  if (history.saved_at > history.applied) {
    history.saved_at = EDITOR_SAVED_POINT_NONE;
  }
  history.actions.resize(history.applied);
  history.actions.push_back(action);
  history.applied = history.actions.size();
}

bool hasUnsavedEditorChanges(const EditorActionHistory& history) {
  return history.applied != history.saved_at;
}

void markEditorChangesSaved(EditorActionHistory& history) {
  history.saved_at = history.applied;
}

void markEditorChangesUnsaved(EditorActionHistory& history) {
  history.saved_at = EDITOR_SAVED_POINT_NONE;
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
  // Nothing to mark: moving the cursor is what says whether the document
  // still matches its file, and undoing back to where a save left it means
  // it does again.
  return true;
}

bool redoEditorAction(EditorActionHistory& history, EditorDocument& document) {
  if (!canRedoEditorAction(history)) {
    return false;
  }
  applyOne(history.actions[history.applied], document);
  ++history.applied;
  return true;
}

size_t editorListSize(const EditorDocument& document,
                      EditorSelectionKind kind) {
  // In `EditorSelectionKind` order, nothing selected first.
  const std::array<size_t, 7> sizes{
      0,
      document.placements.size(),
      document.lights.size(),
      document.player_starts.size(),
      document.waypoints.size(),
      document.emitters.size(),
      document.sprites.size(),
  };
  static_assert(static_cast<size_t>(EditorSelectionKind::SPRITE) + 1 == 7,
                "every list the document holds has a size here");
  const auto at = static_cast<size_t>(kind);
  return at < sizes.size() ? sizes[at] : 0;
}

std::optional<EditorAction> editorDeleteAction(const EditorDocument& document,
                                               EditorSelection selection) {
  if (selection.index >= editorListSize(document, selection.kind)) {
    return std::nullopt;
  }
  EditorAction action{.kind = removalKind(selection.kind),
                      .index = selection.index};
  captureEntry(document, action);
  return action;
}

EditorSelection editorSelectionAfterUndo(const EditorAction& action,
                                         EditorSelection selection) {
  // Painting numbers nothing, so whatever was selected still is.
  if (action.kind == EditorActionKind::PAINT_GROUND) {
    return selection;
  }
  const EditorSelectionKind list = actionList(action.kind);
  switch (invertedEdit(appliedEdit(action.kind))) {
    case ListEdit::ERASE:
      // The entry is gone; anything numbered after it moved down one.
      return selectionAfterErase(list, action.index, selection);
    case ListEdit::INSERT:
    case ListEdit::WRITE:
      // Show what just came back, or what just moved back, so a reverted
      // edit is visible rather than something to go looking for.
      return {list, action.index};
  }
  return selection;
}

EditorSelection editorSelectionAfterRedo(const EditorAction& action,
                                         EditorSelection selection) {
  if (action.kind == EditorActionKind::PAINT_GROUND) {
    return selection;
  }
  const EditorSelectionKind list = actionList(action.kind);
  switch (appliedEdit(action.kind)) {
    case ListEdit::INSERT:
      return selectionAfterInsert(list, action.index, selection);
    case ListEdit::ERASE:
      return selectionAfterErase(list, action.index, selection);
    case ListEdit::WRITE:
      return {list, action.index};
  }
  return selection;
}

void clearEditorActions(EditorActionHistory& history) {
  // Whether the document matches its file survives the clear; where in a
  // list of actions that was true does not, because there is no longer a
  // list. A rescan clears the history and keeps the document it describes.
  const bool saved = !hasUnsavedEditorChanges(history);
  history.actions.clear();
  history.applied = 0;
  history.saved_at = saved ? 0 : EDITOR_SAVED_POINT_NONE;
}

}  // namespace eng::editor
