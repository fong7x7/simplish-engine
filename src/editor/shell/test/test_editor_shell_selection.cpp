#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-ground-ops.h>
#include <editor/shell/editor-menu-availability.h>
#include <editor/shell/editor-shell-selection.h>

using namespace eng;
using namespace eng::editor;

namespace {

/// A document with a 3 × 2 patch of sand and, beside it, a grass cell.
EditorShellState sandAndGrass() {
  EditorShellState state;
  paintEditorGround(state.document.ground, {0, 0, 3, 2}, 3);
  state.document.ground.set({3, 0}, 1);
  return state;
}

}  // namespace

TEST_CASE("clicking painted ground selects its whole patch") {
  EditorShellState state = sandAndGrass();

  REQUIRE(selectEditorGround(state, {1, 1}));

  REQUIRE(state.selection.kind == EditorSelectionKind::GROUND);
  REQUIRE(state.ground_selection.size() == 6);
  REQUIRE(editorSelectedTerrain(state) == uint8_t{3});
  REQUIRE(editorSelectableCount(state, EditorSelectionKind::GROUND) == 1);
}

TEST_CASE("bare ground selects nothing, and leaves the selection alone") {
  EditorShellState state = sandAndGrass();
  state.selection = {EditorSelectionKind::LIGHT, 0};

  REQUIRE_FALSE(selectEditorGround(state, {9, 9}));
  REQUIRE(state.selection.kind == EditorSelectionKind::LIGHT);
  REQUIRE_FALSE(editorSelectedTerrain(state).has_value());
}

TEST_CASE("repainting a patch is one edit over exactly its cells") {
  EditorShellState state = sandAndGrass();
  (void)selectEditorGround(state, {0, 0});

  const auto action =
      editorGroundRepaint(state.document, state.ground_selection, 6);

  REQUIRE(action.has_value());
  REQUIRE(action->ground.size() == 6);
  // The grass beside it is not touched, and painting what is there is no
  // edit.
  REQUIRE_FALSE(editorGroundRepaint(state.document, state.ground_selection, 3));
}

TEST_CASE("Delete is live on a selected patch of ground") {
  EditorShellState state = sandAndGrass();
  state.project.loaded = true;
  REQUIRE_FALSE(
      editorMenuCommandEnabled(state, EditorMenuCommand::DELETE_SELECTION));
  (void)selectEditorGround(state, {0, 0});
  REQUIRE(editorMenuCommandEnabled(state, EditorMenuCommand::DELETE_SELECTION));
}
