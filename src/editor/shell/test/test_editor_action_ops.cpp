#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-player-start-ops.h>
#include <optional>
#include <vector>

using namespace eng::editor;

namespace {

/// The history and the document it describes, edited together the way the
/// editor edits them.
struct HistoryFixture {
  EditorActionHistory history;
  EditorDocument document;

  /// The placements, which most of these tests are about.
  [[nodiscard]] std::vector<EditorPlacement>& placements() {
    return document.placements;
  }

  /// Place asset @p asset at the end of the list, as a drop does.
  void place(size_t asset) {
    performEditorAction(history, document,
                        {.kind = EditorActionKind::PLACE_ASSET,
                         .index = document.placements.size(),
                         .placement = {.asset = asset}});
  }

  /// Move the placement at @p index, as a finished property edit does.
  void move(size_t index, float x) {
    EditorPlacement moved = document.placements[index];
    moved.position.x = x;
    performEditorAction(history, document,
                        {.kind = EditorActionKind::TRANSFORM_PLACEMENT,
                         .index = index,
                         .placement = moved,
                         .prior = document.placements[index]});
  }

  /// Add a light at the end of the list, as dropping one does.
  void addLight(EditorLightKind kind) {
    performEditorAction(history, document,
                        {.kind = EditorActionKind::ADD_LIGHT,
                         .index = document.lights.size(),
                         .light = makeEditorLight(kind, {0.0f, 0.0f, 3.0f})});
  }

  /// Add a start for @p player at the end of the list, as dropping one
  /// does.
  void addStart(uint8_t player) {
    performEditorAction(history, document,
                        {.kind = EditorActionKind::ADD_PLAYER_START,
                         .index = document.player_starts.size(),
                         .player_start = makeEditorPlayerStart(player, {})});
  }

  /// Give the start at @p index to @p player, as a finished property edit
  /// does.
  void reassign(size_t index, uint8_t player) {
    EditorPlayerStart changed = document.player_starts[index];
    changed.player = player;
    performEditorAction(history, document,
                        {.kind = EditorActionKind::TRANSFORM_PLAYER_START,
                         .index = index,
                         .player_start = changed,
                         .player_start_prior = document.player_starts[index]});
  }

  /// Dim the light at @p index, as a finished property edit does.
  void dim(size_t index, float intensity) {
    EditorLight dimmed = document.lights[index];
    dimmed.intensity = intensity;
    performEditorAction(history, document,
                        {.kind = EditorActionKind::TRANSFORM_LIGHT,
                         .index = index,
                         .light = dimmed,
                         .light_prior = document.lights[index]});
  }

  /// Remove whatever @p selection names, as the Delete key does. Returns
  /// the action recorded, so a test can ask where the selection lands.
  EditorAction remove(EditorSelection selection) {
    const std::optional<EditorAction> action =
        editorDeleteAction(document, selection);
    REQUIRE(action.has_value());
    performEditorAction(history, document, *action);
    return *action;
  }

  bool undo() { return undoEditorAction(history, document); }
  bool redo() { return redoEditorAction(history, document); }

  /// The asset indices currently placed, in order.
  [[nodiscard]] std::vector<size_t> assets() const {
    std::vector<size_t> out;
    for (const EditorPlacement& placement : document.placements) {
      out.push_back(placement.asset);
    }
    return out;
  }
};

/// A selection of the placement at @p index.
EditorSelection placementAt(size_t index) {
  return {EditorSelectionKind::PLACEMENT, index};
}

/// A selection of the light at @p index.
EditorSelection lightAt(size_t index) {
  return {EditorSelectionKind::LIGHT, index};
}

/// A selection of the player start at @p index.
EditorSelection startAt(size_t index) {
  return {EditorSelectionKind::PLAYER_START, index};
}

/// Whether two selections name the same entry of the same list.
bool sameSelection(EditorSelection a, EditorSelection b) {
  return a.kind == b.kind && a.index == b.index;
}

}  // namespace

TEST_CASE("a fresh history has nothing to undo or redo") {
  const EditorActionHistory history;
  REQUIRE_FALSE(canUndoEditorAction(history));
  REQUIRE_FALSE(canRedoEditorAction(history));
}

TEST_CASE("performing an action applies it and makes it undoable") {
  HistoryFixture fx;
  fx.place(7);

  REQUIRE(fx.assets() == std::vector<size_t>{7});
  REQUIRE(canUndoEditorAction(fx.history));
  REQUIRE_FALSE(canRedoEditorAction(fx.history));
}

TEST_CASE("undo reverts the newest action and redo reapplies it") {
  HistoryFixture fx;
  fx.place(1);

  REQUIRE(fx.undo());
  REQUIRE(fx.placements().empty());
  REQUIRE_FALSE(canUndoEditorAction(fx.history));
  REQUIRE(canRedoEditorAction(fx.history));

  REQUIRE(fx.redo());
  REQUIRE(fx.assets() == std::vector<size_t>{1});
}

TEST_CASE("a redone placement comes back where it was") {
  HistoryFixture fx;
  performEditorAction(fx.history, fx.document,
                      {.kind = EditorActionKind::PLACE_ASSET,
                       .index = 0,
                       .placement = {.asset = 3, .position = {2.0f, -5.0f}}});
  REQUIRE(fx.undo());
  REQUIRE(fx.redo());

  REQUIRE(fx.placements().size() == 1);
  REQUIRE(fx.placements()[0].asset == 3);
  REQUIRE(fx.placements()[0].position.x == 2.0f);
  REQUIRE(fx.placements()[0].position.y == -5.0f);
}

TEST_CASE("actions undo newest-first and redo oldest-first") {
  HistoryFixture fx;
  fx.place(1);
  fx.place(2);
  fx.place(3);

  REQUIRE(fx.undo());
  REQUIRE(fx.undo());
  REQUIRE(fx.assets() == std::vector<size_t>{1});

  REQUIRE(fx.redo());
  REQUIRE(fx.assets() == std::vector<size_t>{1, 2});
  REQUIRE(fx.redo());
  REQUIRE(fx.assets() == std::vector<size_t>{1, 2, 3});
}

TEST_CASE("undo past the oldest action does nothing") {
  HistoryFixture fx;
  fx.place(1);
  REQUIRE(fx.undo());

  REQUIRE_FALSE(fx.undo());
  REQUIRE(fx.placements().empty());
  REQUIRE(canRedoEditorAction(fx.history));
}

TEST_CASE("redo past the newest action does nothing") {
  HistoryFixture fx;
  fx.place(1);

  REQUIRE_FALSE(fx.redo());
  REQUIRE(fx.assets() == std::vector<size_t>{1});
}

TEST_CASE("a new action after an undo drops what was waiting to be redone") {
  HistoryFixture fx;
  fx.place(1);
  fx.place(2);
  REQUIRE(fx.undo());
  REQUIRE(canRedoEditorAction(fx.history));

  fx.place(9);

  REQUIRE_FALSE(canRedoEditorAction(fx.history));
  REQUIRE(fx.assets() == std::vector<size_t>{1, 9});
  REQUIRE(fx.history.actions.size() == 2);
}

TEST_CASE("the history is unbounded within a session") {
  HistoryFixture fx;
  constexpr size_t COUNT = 500;
  for (size_t i = 0; i < COUNT; ++i) {
    fx.place(i);
  }
  REQUIRE(fx.history.actions.size() == COUNT);

  for (size_t i = 0; i < COUNT; ++i) {
    REQUIRE(fx.undo());
  }
  REQUIRE(fx.placements().empty());
}

TEST_CASE("clearing forgets applied and reverted actions alike") {
  HistoryFixture fx;
  fx.place(1);
  fx.place(2);
  REQUIRE(fx.undo());

  clearEditorActions(fx.history);

  REQUIRE_FALSE(canUndoEditorAction(fx.history));
  REQUIRE_FALSE(canRedoEditorAction(fx.history));
  REQUIRE(fx.history.actions.empty());
}

TEST_CASE("a transform is applied when it is recorded") {
  HistoryFixture fixture;
  fixture.place(0);
  fixture.move(0, 4.5f);
  REQUIRE(fixture.placements()[0].position.x == 4.5f);
}

TEST_CASE("undoing a transform puts the placement back where it was") {
  HistoryFixture fixture;
  fixture.place(0);
  fixture.move(0, 4.5f);

  REQUIRE(fixture.undo());
  REQUIRE(fixture.placements()[0].position.x == 0.0f);
  // The placement itself is still there: a transform moves one, it does not
  // add or remove one.
  REQUIRE(fixture.placements().size() == 1);
}

TEST_CASE("redoing a transform moves the placement again") {
  HistoryFixture fixture;
  fixture.place(0);
  fixture.move(0, 4.5f);
  REQUIRE(fixture.undo());

  REQUIRE(fixture.redo());
  REQUIRE(fixture.placements()[0].position.x == 4.5f);
}

TEST_CASE("a run of transforms undoes one gesture at a time") {
  HistoryFixture fixture;
  fixture.place(0);
  fixture.move(0, 1.0f);
  fixture.move(0, 2.0f);
  fixture.move(0, 3.0f);

  REQUIRE(fixture.undo());
  REQUIRE(fixture.placements()[0].position.x == 2.0f);
  REQUIRE(fixture.undo());
  REQUIRE(fixture.placements()[0].position.x == 1.0f);
  REQUIRE(fixture.undo());
  REQUIRE(fixture.placements()[0].position.x == 0.0f);
}

TEST_CASE("a transform names a placement that is no longer there") {
  // The history and the document have diverged, which is a bug elsewhere.
  // Writing past the end would be a worse answer than doing nothing.
  EditorDocument document;
  EditorActionHistory history;
  performEditorAction(history, document,
                      {.kind = EditorActionKind::TRANSFORM_PLACEMENT,
                       .index = 3,
                       .placement = {},
                       .prior = {}});
  REQUIRE(document.placements.empty());
}

TEST_CASE("undoing a placement drops a selection of it") {
  const EditorAction placed{
      .kind = EditorActionKind::PLACE_ASSET, .index = 2, .placement = {}};
  REQUIRE(editorSelectionAfterUndo(placed, placementAt(2)).kind ==
          EditorSelectionKind::NONE);
}

TEST_CASE("undoing a placement renumbers a selection after it") {
  const EditorAction placed{
      .kind = EditorActionKind::PLACE_ASSET, .index = 1, .placement = {}};
  // Everything past the removed entry slid down one, and the selection has
  // to slide with it or it names a different placement.
  REQUIRE(sameSelection(editorSelectionAfterUndo(placed, placementAt(3)),
                        placementAt(2)));
  REQUIRE(sameSelection(editorSelectionAfterUndo(placed, placementAt(0)),
                        placementAt(0)));
  REQUIRE(editorSelectionAfterUndo(placed, EditorSelection{}).kind ==
          EditorSelectionKind::NONE);
}

TEST_CASE("undoing a placement leaves a selected light alone") {
  const EditorAction placed{
      .kind = EditorActionKind::PLACE_ASSET, .index = 0, .placement = {}};
  // The two lists are numbered separately, so an edit to one says nothing
  // about a selection in the other.
  REQUIRE(
      sameSelection(editorSelectionAfterUndo(placed, lightAt(2)), lightAt(2)));
}

TEST_CASE("redoing a placement renumbers a selection at or after it") {
  const EditorAction placed{
      .kind = EditorActionKind::PLACE_ASSET, .index = 1, .placement = {}};
  REQUIRE(sameSelection(editorSelectionAfterRedo(placed, placementAt(1)),
                        placementAt(2)));
  REQUIRE(sameSelection(editorSelectionAfterRedo(placed, placementAt(0)),
                        placementAt(0)));
}

TEST_CASE("undoing or redoing a transform selects what it moved") {
  const EditorAction moved{.kind = EditorActionKind::TRANSFORM_PLACEMENT,
                           .index = 4,
                           .placement = {},
                           .prior = {}};
  // A reverted move nobody can see is a reverted move nobody trusts.
  REQUIRE(sameSelection(editorSelectionAfterUndo(moved, EditorSelection{}),
                        placementAt(4)));
  REQUIRE(sameSelection(editorSelectionAfterRedo(moved, placementAt(1)),
                        placementAt(4)));
}

TEST_CASE("adding a light is undone and redone like a placement") {
  HistoryFixture fx;
  fx.addLight(EditorLightKind::POINT);
  REQUIRE(fx.document.lights.size() == 1);
  REQUIRE(fx.document.lights[0].kind == EditorLightKind::POINT);

  REQUIRE(fx.undo());
  REQUIRE(fx.document.lights.empty());
  REQUIRE(fx.redo());
  REQUIRE(fx.document.lights.size() == 1);
}

TEST_CASE("a light edit is undone without touching the placements") {
  HistoryFixture fx;
  fx.place(3);
  fx.addLight(EditorLightKind::DIRECTIONAL);
  fx.dim(0, 0.25f);
  REQUIRE(fx.document.lights[0].intensity == 0.25f);

  REQUIRE(fx.undo());
  REQUIRE(fx.document.lights[0].intensity == 1.0f);
  // The placement was never part of any of that.
  REQUIRE(fx.assets() == std::vector<size_t>{3});
}

TEST_CASE("undoing a light drops a selection of it") {
  const EditorAction added{.kind = EditorActionKind::ADD_LIGHT, .index = 1};
  REQUIRE(editorSelectionAfterUndo(added, lightAt(1)).kind ==
          EditorSelectionKind::NONE);
  REQUIRE(
      sameSelection(editorSelectionAfterUndo(added, lightAt(2)), lightAt(1)));
  // A placement selected while a light is undone stays where it was.
  REQUIRE(sameSelection(editorSelectionAfterUndo(added, placementAt(2)),
                        placementAt(2)));
}

TEST_CASE("undoing or redoing a light edit selects the light") {
  const EditorAction dimmed{.kind = EditorActionKind::TRANSFORM_LIGHT,
                            .index = 2};
  REQUIRE(sameSelection(editorSelectionAfterUndo(dimmed, EditorSelection{}),
                        lightAt(2)));
  REQUIRE(sameSelection(editorSelectionAfterRedo(dimmed, placementAt(0)),
                        lightAt(2)));
}

TEST_CASE("a fresh history has nothing to save") {
  HistoryFixture fx;

  REQUIRE_FALSE(hasUnsavedEditorChanges(fx.history));
}

TEST_CASE("an edit leaves changes to save, and a save takes them away") {
  HistoryFixture fx;
  fx.place(0);
  REQUIRE(hasUnsavedEditorChanges(fx.history));

  markEditorChangesSaved(fx.history);
  REQUIRE_FALSE(hasUnsavedEditorChanges(fx.history));

  fx.move(0, 4.0f);
  REQUIRE(hasUnsavedEditorChanges(fx.history));
}

TEST_CASE("undoing everything since the save takes the marker off again") {
  HistoryFixture fx;
  fx.place(0);
  markEditorChangesSaved(fx.history);
  fx.place(1);
  fx.move(1, 7.0f);
  REQUIRE(hasUnsavedEditorChanges(fx.history));

  REQUIRE(undoEditorAction(fx.history, fx.document));
  REQUIRE(hasUnsavedEditorChanges(fx.history));

  // Back to the document the file holds, so there is nothing to save.
  REQUIRE(undoEditorAction(fx.history, fx.document));
  REQUIRE_FALSE(hasUnsavedEditorChanges(fx.history));
  REQUIRE(fx.placements().size() == 1);
}

TEST_CASE("redoing past the save point needs saving again") {
  HistoryFixture fx;
  fx.place(0);
  markEditorChangesSaved(fx.history);
  fx.place(1);
  REQUIRE(undoEditorAction(fx.history, fx.document));
  REQUIRE_FALSE(hasUnsavedEditorChanges(fx.history));

  REQUIRE(redoEditorAction(fx.history, fx.document));
  REQUIRE(hasUnsavedEditorChanges(fx.history));
}

TEST_CASE("undoing to a saved document without saving it stays unsaved") {
  HistoryFixture fx;
  fx.place(0);
  fx.place(1);
  markEditorChangesSaved(fx.history);

  // The file holds both; memory now holds one.
  REQUIRE(undoEditorAction(fx.history, fx.document));
  REQUIRE(hasUnsavedEditorChanges(fx.history));
}

TEST_CASE("a different edit after an undo cannot reach the saved document") {
  HistoryFixture fx;
  fx.place(0);
  fx.place(1);
  markEditorChangesSaved(fx.history);
  REQUIRE(undoEditorAction(fx.history, fx.document));

  // This discards the redo tail the save was recorded against, so the
  // cursor lands back on the same number describing a different document.
  fx.place(2);
  REQUIRE(fx.history.applied == 2);
  REQUIRE(hasUnsavedEditorChanges(fx.history));

  // And no amount of undoing gets back to what was written.
  REQUIRE(undoEditorAction(fx.history, fx.document));
  REQUIRE(hasUnsavedEditorChanges(fx.history));
  REQUIRE(undoEditorAction(fx.history, fx.document));
  REQUIRE(hasUnsavedEditorChanges(fx.history));
}

TEST_CASE("an undo with nothing to undo changes nothing to save") {
  HistoryFixture fx;
  fx.place(0);
  markEditorChangesSaved(fx.history);

  REQUIRE(undoEditorAction(fx.history, fx.document));
  markEditorChangesSaved(fx.history);
  // The history is empty now, so this reverts nothing and dirties nothing.
  REQUIRE_FALSE(undoEditorAction(fx.history, fx.document));
  REQUIRE_FALSE(hasUnsavedEditorChanges(fx.history));
}

TEST_CASE("forgetting the actions keeps a saved document saved") {
  HistoryFixture fx;
  fx.place(0);
  markEditorChangesSaved(fx.history);

  clearEditorActions(fx.history);
  REQUIRE_FALSE(hasUnsavedEditorChanges(fx.history));
}

TEST_CASE("forgetting the actions says nothing about the file") {
  HistoryFixture fx;
  fx.place(0);

  // A rescan clears the history and keeps the document, so the document is
  // still the one that has not been written.
  clearEditorActions(fx.history);
  REQUIRE(hasUnsavedEditorChanges(fx.history));
  REQUIRE(fx.placements().size() == 1);
}

TEST_CASE("a change that is not an action still needs saving") {
  HistoryFixture fx;
  markEditorChangesSaved(fx.history);

  markEditorChangesUnsaved(fx.history);
  REQUIRE(hasUnsavedEditorChanges(fx.history));
}

TEST_CASE("editorDeleteAction names the entry the selection is on") {
  HistoryFixture fx;
  fx.place(4);
  fx.place(5);

  const std::optional<EditorAction> action =
      editorDeleteAction(fx.document, placementAt(1));

  REQUIRE(action.has_value());
  REQUIRE(action->kind == EditorActionKind::REMOVE_PLACEMENT);
  REQUIRE(action->index == 1);
  // The entry travels in the action, which is what lets undo put back the
  // one that was there rather than a fresh one at its index.
  REQUIRE(action->placement.asset == 5);
}

TEST_CASE("editorDeleteAction has nothing to remove for an empty selection") {
  HistoryFixture fx;
  fx.place(0);

  REQUIRE_FALSE(editorDeleteAction(fx.document, {}).has_value());
  REQUIRE_FALSE(editorDeleteAction(fx.document, placementAt(1)).has_value());
  REQUIRE_FALSE(editorDeleteAction(fx.document, lightAt(0)).has_value());
}

TEST_CASE("removing a placement takes it out and undo puts it back") {
  HistoryFixture fx;
  fx.place(1);
  fx.place(2);
  fx.place(3);

  (void)fx.remove(placementAt(1));
  REQUIRE(fx.assets() == std::vector<size_t>{1, 3});

  REQUIRE(fx.undo());
  REQUIRE(fx.assets() == std::vector<size_t>{1, 2, 3});

  REQUIRE(fx.redo());
  REQUIRE(fx.assets() == std::vector<size_t>{1, 3});
}

TEST_CASE("an undone removal restores the entry as it was, not a fresh one") {
  HistoryFixture fx;
  performEditorAction(fx.history, fx.document,
                      {.kind = EditorActionKind::PLACE_ASSET,
                       .index = 0,
                       .placement = {.asset = 9, .position = {4.0f, 6.0f}}});

  (void)fx.remove(placementAt(0));
  REQUIRE(fx.undo());

  REQUIRE(fx.placements().size() == 1);
  REQUIRE(fx.placements()[0].asset == 9);
  REQUIRE(fx.placements()[0].position.x == 4.0f);
  REQUIRE(fx.placements()[0].position.y == 6.0f);
}

TEST_CASE("removing a light takes it out of the other list") {
  HistoryFixture fx;
  fx.place(0);
  fx.addLight(EditorLightKind::POINT);
  fx.addLight(EditorLightKind::DIRECTIONAL);

  (void)fx.remove(lightAt(0));

  REQUIRE(fx.document.lights.size() == 1);
  REQUIRE(fx.document.lights[0].kind == EditorLightKind::DIRECTIONAL);
  // The other list is numbered separately, so it is untouched.
  REQUIRE(fx.placements().size() == 1);

  REQUIRE(fx.undo());
  REQUIRE(fx.document.lights.size() == 2);
  REQUIRE(fx.document.lights[0].kind == EditorLightKind::POINT);
}

TEST_CASE("a removal drops a selection on what it removed") {
  HistoryFixture fx;
  fx.place(1);
  fx.place(2);
  const EditorAction action = fx.remove(placementAt(1));

  REQUIRE(sameSelection(editorSelectionAfterRedo(action, placementAt(1)), {}));
}

TEST_CASE("a removal renumbers a selection that sat after it") {
  HistoryFixture fx;
  fx.place(1);
  fx.place(2);
  fx.place(3);
  const EditorAction action = fx.remove(placementAt(0));

  REQUIRE(sameSelection(editorSelectionAfterRedo(action, placementAt(2)),
                        placementAt(1)));
  // A selection in the lights is numbered by its own list, so it stays.
  REQUIRE(
      sameSelection(editorSelectionAfterRedo(action, lightAt(0)), lightAt(0)));
}

TEST_CASE("undoing a removal selects what came back") {
  HistoryFixture fx;
  fx.place(1);
  fx.place(2);
  const EditorAction action = fx.remove(placementAt(0));

  // Shown rather than left to be looked for, the same as an undone move.
  REQUIRE(sameSelection(editorSelectionAfterUndo(action, {}), placementAt(0)));
}

TEST_CASE("adding a player start is undone and redone like a placement") {
  HistoryFixture fx;
  fx.addStart(2);

  REQUIRE(fx.document.player_starts.size() == 1);
  REQUIRE(fx.undo());
  REQUIRE(fx.document.player_starts.empty());
  REQUIRE(fx.redo());
  REQUIRE(fx.document.player_starts.size() == 1);
  REQUIRE(fx.document.player_starts[0].player == 2);
}

TEST_CASE("giving a start to another player is undone on its own list") {
  HistoryFixture fx;
  fx.place(0);
  fx.addLight(EditorLightKind::POINT);
  fx.addStart(1);
  fx.reassign(0, 3);

  REQUIRE(fx.document.player_starts[0].player == 3);
  REQUIRE(fx.undo());
  REQUIRE(fx.document.player_starts[0].player == 1);
  // The other lists are numbered separately and untouched.
  REQUIRE(fx.placements().size() == 1);
  REQUIRE(fx.document.lights.size() == 1);
}

TEST_CASE("removing a player start takes it out and undo puts it back") {
  HistoryFixture fx;
  fx.addStart(1);
  fx.addStart(2);

  const EditorAction action = fx.remove(startAt(0));

  REQUIRE(action.kind == EditorActionKind::REMOVE_PLAYER_START);
  REQUIRE(fx.document.player_starts.size() == 1);
  REQUIRE(fx.document.player_starts[0].player == 2);
  REQUIRE(sameSelection(editorSelectionAfterRedo(action, startAt(0)), {}));
  REQUIRE(fx.undo());
  REQUIRE(fx.document.player_starts[0].player == 1);
  REQUIRE(sameSelection(editorSelectionAfterUndo(action, {}), startAt(0)));
}

TEST_CASE("undoing a player start leaves a selected light alone") {
  HistoryFixture fx;
  fx.addLight(EditorLightKind::POINT);
  fx.addStart(1);
  const EditorAction added = fx.history.actions.back();

  REQUIRE(fx.undo());
  REQUIRE(
      sameSelection(editorSelectionAfterUndo(added, lightAt(0)), lightAt(0)));
}

TEST_CASE("the list a selection names is measured by its own kind") {
  HistoryFixture fx;
  fx.place(0);
  fx.addStart(1);
  fx.addStart(2);

  REQUIRE(editorListSize(fx.document, EditorSelectionKind::PLACEMENT) == 1);
  REQUIRE(editorListSize(fx.document, EditorSelectionKind::LIGHT) == 0);
  REQUIRE(editorListSize(fx.document, EditorSelectionKind::PLAYER_START) == 2);
  REQUIRE(editorListSize(fx.document, EditorSelectionKind::NONE) == 0);
  REQUIRE_FALSE(editorDeleteAction(fx.document, startAt(2)).has_value());
}
