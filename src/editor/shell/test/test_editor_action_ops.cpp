#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <editor/shell/editor-action-ops.h>
#include <vector>

using namespace eng::editor;

namespace {

/// The history and the document it describes, edited together the way the
/// editor edits them.
struct HistoryFixture {
  EditorActionHistory history;
  std::vector<EditorPlacement> placements;

  /// Place asset @p asset at the end of the list, as `dropAsset` does.
  void place(size_t asset) {
    performEditorAction(history, placements,
                        {.kind = EditorActionKind::PLACE_ASSET,
                         .index = placements.size(),
                         .placement = {asset, {0.0f, 0.0f}, {}}});
  }

  /// Move the placement at @p index, as a finished property edit does.
  void move(size_t index, float x) {
    EditorPlacement moved = placements[index];
    moved.position.x = x;
    performEditorAction(history, placements,
                        {.kind = EditorActionKind::TRANSFORM_PLACEMENT,
                         .index = index,
                         .placement = moved,
                         .prior = placements[index]});
  }

  bool undo() { return undoEditorAction(history, placements); }
  bool redo() { return redoEditorAction(history, placements); }

  /// The asset indices currently placed, in order.
  [[nodiscard]] std::vector<size_t> assets() const {
    std::vector<size_t> out;
    for (const EditorPlacement& placement : placements) {
      out.push_back(placement.asset);
    }
    return out;
  }
};

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
  REQUIRE(fx.placements.empty());
  REQUIRE_FALSE(canUndoEditorAction(fx.history));
  REQUIRE(canRedoEditorAction(fx.history));

  REQUIRE(fx.redo());
  REQUIRE(fx.assets() == std::vector<size_t>{1});
}

TEST_CASE("a redone placement comes back where it was") {
  HistoryFixture fx;
  performEditorAction(fx.history, fx.placements,
                      {.kind = EditorActionKind::PLACE_ASSET,
                       .index = 0,
                       .placement = {3, {2.0f, -5.0f}}});
  REQUIRE(fx.undo());
  REQUIRE(fx.redo());

  REQUIRE(fx.placements.size() == 1);
  REQUIRE(fx.placements[0].asset == 3);
  REQUIRE(fx.placements[0].position.x == 2.0f);
  REQUIRE(fx.placements[0].position.y == -5.0f);
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
  REQUIRE(fx.placements.empty());
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
  REQUIRE(fx.placements.empty());
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
  REQUIRE(fixture.placements[0].position.x == 4.5f);
}

TEST_CASE("undoing a transform puts the placement back where it was") {
  HistoryFixture fixture;
  fixture.place(0);
  fixture.move(0, 4.5f);

  REQUIRE(fixture.undo());
  REQUIRE(fixture.placements[0].position.x == 0.0f);
  // The placement itself is still there: a transform moves one, it does not
  // add or remove one.
  REQUIRE(fixture.placements.size() == 1);
}

TEST_CASE("redoing a transform moves the placement again") {
  HistoryFixture fixture;
  fixture.place(0);
  fixture.move(0, 4.5f);
  REQUIRE(fixture.undo());

  REQUIRE(fixture.redo());
  REQUIRE(fixture.placements[0].position.x == 4.5f);
}

TEST_CASE("a run of transforms undoes one gesture at a time") {
  HistoryFixture fixture;
  fixture.place(0);
  fixture.move(0, 1.0f);
  fixture.move(0, 2.0f);
  fixture.move(0, 3.0f);

  REQUIRE(fixture.undo());
  REQUIRE(fixture.placements[0].position.x == 2.0f);
  REQUIRE(fixture.undo());
  REQUIRE(fixture.placements[0].position.x == 1.0f);
  REQUIRE(fixture.undo());
  REQUIRE(fixture.placements[0].position.x == 0.0f);
}

TEST_CASE("a transform names a placement that is no longer there") {
  // The history and the document have diverged, which is a bug elsewhere.
  // Writing past the end would be a worse answer than doing nothing.
  std::vector<EditorPlacement> placements;
  EditorActionHistory history;
  performEditorAction(history, placements,
                      {.kind = EditorActionKind::TRANSFORM_PLACEMENT,
                       .index = 3,
                       .placement = {},
                       .prior = {}});
  REQUIRE(placements.empty());
}

TEST_CASE("undoing a placement drops a selection of it") {
  const EditorAction placed{
      .kind = EditorActionKind::PLACE_ASSET, .index = 2, .placement = {}};
  REQUIRE(editorSelectionAfterUndo(placed, 2) == EDITOR_PLACEMENT_NONE);
}

TEST_CASE("undoing a placement renumbers a selection after it") {
  const EditorAction placed{
      .kind = EditorActionKind::PLACE_ASSET, .index = 1, .placement = {}};
  // Everything past the removed entry slid down one, and the selection has
  // to slide with it or it names a different placement.
  REQUIRE(editorSelectionAfterUndo(placed, 3) == 2);
  REQUIRE(editorSelectionAfterUndo(placed, 0) == 0);
  REQUIRE(editorSelectionAfterUndo(placed, EDITOR_PLACEMENT_NONE) ==
          EDITOR_PLACEMENT_NONE);
}

TEST_CASE("redoing a placement renumbers a selection at or after it") {
  const EditorAction placed{
      .kind = EditorActionKind::PLACE_ASSET, .index = 1, .placement = {}};
  REQUIRE(editorSelectionAfterRedo(placed, 1) == 2);
  REQUIRE(editorSelectionAfterRedo(placed, 0) == 0);
}

TEST_CASE("undoing or redoing a transform selects what it moved") {
  const EditorAction moved{.kind = EditorActionKind::TRANSFORM_PLACEMENT,
                           .index = 4,
                           .placement = {},
                           .prior = {}};
  // A reverted move nobody can see is a reverted move nobody trusts.
  REQUIRE(editorSelectionAfterUndo(moved, EDITOR_PLACEMENT_NONE) == 4);
  REQUIRE(editorSelectionAfterRedo(moved, 1) == 4);
}
