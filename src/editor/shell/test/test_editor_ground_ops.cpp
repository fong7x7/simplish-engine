#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-ground-ops.h>

using namespace eng;
using namespace eng::editor;

TEST_CASE("an odd brush is centred on the tile under the point") {
  REQUIRE(editorBrushRect({4.5f, -2.2f}, 1) == GroundRect{4, -3, 1, 1});
  REQUIRE(editorBrushRect({4.5f, -2.2f}, 3) == GroundRect{3, -4, 3, 3});
}

TEST_CASE("an even brush reaches one further south-west") {
  REQUIRE(editorBrushRect({0.5f, 0.5f}, 2) == GroundRect{-1, -1, 2, 2});
}

TEST_CASE("the brush is held to the sizes the editor offers") {
  REQUIRE(editorBrushRect({0.0f, 0.0f}, 0).width == EDITOR_BRUSH_SIZE_MIN);
  REQUIRE(editorBrushRect({0.0f, 0.0f}, 99).width == EDITOR_BRUSH_SIZE_MAX);
}

TEST_CASE("painting a rectangle fills every cell of it") {
  GroundGrid grid;
  REQUIRE(paintEditorGround(grid, {1, 1, 3, 2}, 2));
  REQUIRE(grid.paintedBounds() == GroundRect{1, 1, 3, 2});
  REQUIRE(grid.at({3, 2}) == 2);
  REQUIRE_FALSE(paintEditorGround(grid, {1, 1, 3, 2}, 2));
}

TEST_CASE("the difference between two grids is the cells that changed") {
  GroundGrid before;
  before.set({0, 0}, 1);
  GroundGrid after = before;
  after.set({0, 0}, 3);
  after.set({5, 5}, 2);

  const std::vector<EditorGroundChange> changes =
      diffEditorGround(before, after);

  REQUIRE(changes ==
          std::vector<EditorGroundChange>{{{0, 0}, 1, 3}, {{5, 5}, 0, 2}});
}

TEST_CASE("changes written back and forth give each grid back") {
  GroundGrid before;
  before.set({2, 2}, 1);
  GroundGrid after = before;
  paintEditorGround(after, {0, 0, 4, 4}, 5);
  const auto changes = diffEditorGround(before, after);

  GroundGrid grid = before;
  applyEditorGroundChanges(grid, changes, EditorGroundSide::AFTER);
  REQUIRE(diffEditorGround(grid, after).empty());
  applyEditorGroundChanges(grid, changes, EditorGroundSide::BEFORE);
  REQUIRE(diffEditorGround(grid, before).empty());
}
