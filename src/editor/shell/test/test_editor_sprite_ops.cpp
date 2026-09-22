#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-sprite-ops.h>
#include <filesystem>
#include <vector>

using Catch::Approx;
using Field = eng::editor::EditorPropertyField;
using namespace eng::editor;

namespace {

/// The sheets a project of two might hold.
std::vector<std::filesystem::path> testSheets() {
  return {std::filesystem::path("sprites/slime.png"),
          std::filesystem::path("props/torch.png")};
}

/// A billboard on the first of those, cut into a @p columns by @p rows
/// grid playing every cell.
EditorSprite gridded(uint16_t columns, uint16_t rows) {
  EditorSprite sprite = makeEditorSprite("sprites/slime.png", {2.5f, 3.5f});
  setEditorSpriteValue(sprite, Field::COLUMNS, columns);
  setEditorSpriteValue(sprite, Field::ROWS, rows);
  return sprite;
}

}  // namespace

TEST_CASE("a new billboard is one frame of its sheet, a tile tall") {
  const EditorSprite sprite =
      makeEditorSprite("sprites/slime.png", {2.5f, 3.5f, 0.0f});

  // One frame rather than a guessed grid: a sheet's shape is something
  // only its author knows, and a guess would show a quarter of a frame.
  REQUIRE(sprite.grid.columns == 1);
  REQUIRE(sprite.grid.rows == 1);
  REQUIRE(sprite.grid.frames == 1);
  REQUIRE(sprite.height == Approx(1.0f));
  REQUIRE(sprite.sheet == "sprites/slime.png");
}

TEST_CASE("a billboard is named for its sheet, without the extension") {
  REQUIRE(editorSpriteName(makeEditorSprite("sprites/slime.png", {})) ==
          "Sprite Billboard · sprites/slime");
  // One with no sheet is still a billboard, and says so.
  REQUIRE(editorSpriteName(makeEditorSprite("", {})) == "Sprite Billboard");
}

TEST_CASE("the Sheet row offers every sheet and marks the one shown") {
  const EditorSheetChoices choices =
      editorSheetChoices(makeEditorSprite("props/torch.png", {}), testSheets());

  REQUIRE(choices.names.size() == 2);
  REQUIRE(choices.paths[0] == "sprites/slime.png");
  REQUIRE(choices.names[0] == "sprites/slime");
  REQUIRE(choices.current == 1);
}

TEST_CASE("a sheet the project no longer holds is offered last, as missing") {
  const EditorSheetChoices choices =
      editorSheetChoices(makeEditorSprite("gone/ghost.png", {}), testSheets());

  // Said rather than silently repointed: a file deleted since the level was
  // saved is a project problem, and rewriting the billboard would hide it.
  REQUIRE(choices.paths.size() == 3);
  REQUIRE(choices.current == 2);
  REQUIRE(choices.names.back() == "gone/ghost (missing)");
  REQUIRE(choices.paths.back() == "gone/ghost.png");
}

TEST_CASE("a project with no sheets offers nothing at all") {
  REQUIRE(editorSheetChoices(makeEditorSprite("", {}), {}).paths.empty());
}

TEST_CASE("cutting a sheet into a grid plays every cell of it") {
  const EditorSprite sprite = gridded(4, 3);

  REQUIRE(editorSpriteValue(sprite, Field::FRAMES) == Approx(12.0f));
}

TEST_CASE("a frame count the designer set survives the grid changing") {
  EditorSprite sprite = gridded(4, 3);
  setEditorSpriteValue(sprite, Field::FRAMES, 10.0f);
  setEditorSpriteValue(sprite, Field::ROWS, 4.0f);

  // Ten was chosen, so ten it stays; the two empty cells of the old grid
  // and the six of the new one are both none of the editor's business.
  REQUIRE(editorSpriteValue(sprite, Field::FRAMES) == Approx(10.0f));
}

TEST_CASE("a frame count is held to the cells the grid has") {
  EditorSprite sprite = gridded(4, 3);
  setEditorSpriteValue(sprite, Field::FRAMES, 11.0f);
  setEditorSpriteValue(sprite, Field::COLUMNS, 2.0f);

  REQUIRE(editorSpriteValue(sprite, Field::FRAMES) == Approx(6.0f));
}

TEST_CASE("a grid and a speed are held to what the panel allows") {
  EditorSprite sprite = makeEditorSprite("sprites/slime.png", {});
  setEditorSpriteValue(sprite, Field::COLUMNS, 0.0f);
  setEditorSpriteValue(sprite, Field::FPS, -4.0f);
  setEditorSpriteValue(sprite, Field::HEIGHT, -1.0f);

  REQUIRE(sprite.grid.columns == 1);
  REQUIRE(sprite.grid.fps == Approx(0.0f));
  REQUIRE(sprite.height == Approx(0.0f));
}

TEST_CASE("a billboard holds its own fields and nobody else's") {
  REQUIRE(editorSpriteHasField(Field::HEIGHT));
  REQUIRE(editorSpriteHasField(Field::POSITION_X));
  // An emitter's, a light's and a placement's are not a billboard's.
  REQUIRE_FALSE(editorSpriteHasField(Field::EMIT_INTERVAL));
  REQUIRE_FALSE(editorSpriteHasField(Field::INTENSITY));
  REQUIRE_FALSE(editorSpriteHasField(Field::SCALE));
}

TEST_CASE("two billboards are the same only to the last bit") {
  const EditorSprite sprite = gridded(4, 3);
  EditorSprite moved = sprite;
  moved.position.x += 0.001f;

  REQUIRE(sameEditorSprite(sprite, sprite));
  REQUIRE_FALSE(sameEditorSprite(sprite, moved));
}

TEST_CASE("the marker box stands on the base, as wide as the sprite draws") {
  EditorSprite sprite = makeEditorSprite("sprites/slime.png", {2.5f, 3.5f});
  setEditorSpriteValue(sprite, Field::HEIGHT, 2.0f);
  const PlacementBounds box = editorSpriteBounds(sprite, 1.5f);

  REQUIRE(box.min.x == Approx(2.5f - 0.75f));
  REQUIRE(box.max.x == Approx(2.5f + 0.75f));
  REQUIRE(box.min.z == Approx(0.0f));
  REQUIRE(box.max.z == Approx(2.0f));
  // Thin in depth, but never nothing: a quad with no depth at all would be
  // a marker the pointer could not hit.
  REQUIRE(box.max.y - box.min.y == Approx(EDITOR_SPRITE_MARKER_DEPTH));
}
