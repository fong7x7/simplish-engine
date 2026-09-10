#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-property-ops.h>
#include <limits>

using namespace eng::editor;

namespace {

/// A document holding a start for each player in @p players.
EditorDocument documentWithStarts(std::initializer_list<uint8_t> players) {
  EditorDocument document;
  for (const uint8_t player : players) {
    document.player_starts.push_back(makeEditorPlayerStart(player, {}));
  }
  return document;
}

}  // namespace

TEST_CASE("a session holds as many players as the simulation has slots") {
  REQUIRE(EDITOR_PLAYER_SLOTS == 4);
}

TEST_CASE("a player slot is rounded to a whole player and clamped") {
  REQUIRE(clampEditorPlayerSlot(2.0f) == 2);
  REQUIRE(clampEditorPlayerSlot(2.4f) == 2);
  REQUIRE(clampEditorPlayerSlot(2.6f) == 3);
  REQUIRE(clampEditorPlayerSlot(0.0f) == 1);
  REQUIRE(clampEditorPlayerSlot(-7.0f) == 1);
  REQUIRE(clampEditorPlayerSlot(9.0f) == 4);
  REQUIRE(clampEditorPlayerSlot(std::numeric_limits<float>::quiet_NaN()) == 1);
}

TEST_CASE("the next slot is the lowest player with no start") {
  REQUIRE(nextEditorPlayerSlot(EditorDocument{}) == 1);
  REQUIRE(nextEditorPlayerSlot(documentWithStarts({1})) == 2);
  REQUIRE(nextEditorPlayerSlot(documentWithStarts({1, 2})) == 3);
  // A gap is filled before the end is extended.
  REQUIRE(nextEditorPlayerSlot(documentWithStarts({1, 3})) == 2);
}

TEST_CASE("once every player has a start the next one is player 1 again") {
  REQUIRE(nextEditorPlayerSlot(documentWithStarts({1, 2, 3, 4})) == 1);
}

TEST_CASE("a new start is for the player it was made for, where it was put") {
  const EditorPlayerStart start = makeEditorPlayerStart(3, {2.5f, 4.5f, 0.0f});

  REQUIRE(start.player == 3);
  REQUIRE(start.position.x == 2.5f);
  REQUIRE(start.position.y == 4.5f);
  REQUIRE(editorPlayerStartName(start) == "Player 3 Start");
}

TEST_CASE("a start is made for a slot the session has, whatever it is asked") {
  REQUIRE(makeEditorPlayerStart(0, {}).player == 1);
  REQUIRE(makeEditorPlayerStart(12, {}).player == 4);
}

TEST_CASE("a start's properties are its player and its position") {
  const EditorPlayerStart start = makeEditorPlayerStart(2, {1.0f, 2.0f, 3.0f});

  REQUIRE(editorPlayerStartValue(start, EditorPropertyField::PLAYER) == 2.0f);
  REQUIRE(editorPlayerStartValue(start, EditorPropertyField::POSITION_Y) ==
          2.0f);
  // Something only a light or a placement has reads as nothing.
  REQUIRE(editorPlayerStartValue(start, EditorPropertyField::RANGE) == 0.0f);
  REQUIRE(editorPlayerStartHasField(EditorPropertyField::PLAYER));
  REQUIRE(editorPlayerStartHasField(EditorPropertyField::POSITION_Z));
  REQUIRE_FALSE(editorPlayerStartHasField(EditorPropertyField::ROTATION_Z));
}

TEST_CASE("writing a start's player holds it to a slot the session has") {
  EditorPlayerStart start = makeEditorPlayerStart(1, {});

  setEditorPlayerStartValue(start, EditorPropertyField::PLAYER, 3.3f);
  REQUIRE(start.player == 3);
  setEditorPlayerStartValue(start, EditorPropertyField::PLAYER, 40.0f);
  REQUIRE(start.player == 4);
}

TEST_CASE("writing a start's position moves it, and other fields do nothing") {
  EditorPlayerStart start = makeEditorPlayerStart(1, {});

  setEditorPlayerStartValue(start, EditorPropertyField::POSITION_X, 6.25f);
  setEditorPlayerStartValue(start, EditorPropertyField::INTENSITY, 9.0f);

  REQUIRE(start.position.x == 6.25f);
  REQUIRE(start.player == 1);
}

TEST_CASE("a start's marker is a column standing on its position") {
  const EditorPlayerStart start = makeEditorPlayerStart(1, {3.5f, 4.5f, 1.0f});
  const PlacementBounds bounds = editorPlayerStartBounds(start);

  REQUIRE(bounds.min.z == 1.0f);
  REQUIRE(bounds.max.z == 1.0f + EDITOR_PLAYER_START_MARKER_HEIGHT);
  REQUIRE(bounds.min.x < 3.5f);
  REQUIRE(bounds.max.x > 3.5f);
  // Narrower than a tile, so two starts on neighbouring tiles pick apart.
  REQUIRE(bounds.max.x - bounds.min.x < 1.0f);
}

TEST_CASE("the player field steps a whole player and writes no decimals") {
  REQUIRE(editorPropertyStep(EditorPropertyField::PLAYER) == 1.0f);
  REQUIRE(normalizeEditorPropertyValue(EditorPropertyField::PLAYER, 2.7f) ==
          3.0f);
  REQUIRE(formatEditorPropertyValue(2.0f, EditorPropertyField::PLAYER) == "2");
}
