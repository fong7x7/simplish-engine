#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-character-figure.h>
#include <editor/shell/editor-player-start-ops.h>

using namespace eng::editor;

TEST_CASE("a start naming a character has its model standing on it") {
  EditorDocument document;
  document.player_starts.push_back(makeEditorPlayerStart(1, {1.5f, 2.5f, 0}));
  document.player_starts.push_back(makeEditorPlayerStart(2, {3.5f, 2.5f, 0}));
  document.player_starts.push_back(makeEditorPlayerStart(3, {5.5f, 2.5f, 0}));
  document.player_starts[1].id = "start_02";
  document.player_starts[1].character = "character:scout";
  // Names a character the table does not have: no figure.
  document.player_starts[2].character = "character:gone";
  const std::vector<eng::game::CharacterDefinition> characters{
      {"scout", "Scout", "mesh:hero", 7.0f, 3}};

  const std::vector<EditorCharacterFigure> figures =
      editorStartFigures(document, characters);

  REQUIRE(figures.size() == 1);
  REQUIRE(figures[0].key == "player_start:start_02");
  REQUIRE(figures[0].model == "mesh:hero");
  REQUIRE(figures[0].feet.x == 3.5f);
  // Facing the way a player spawns aiming, and standing still.
  REQUIRE(figures[0].aim.x == 1.0f);
  REQUIRE(figures[0].aim.y == 0.0f);
  REQUIRE(figures[0].gait == EditorCharacterGait::STILL);
}
