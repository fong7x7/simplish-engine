#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-character-figure.h>
#include <editor/shell/editor-player-start-ops.h>

using namespace eng::editor;

TEST_CASE("only a start with a character has a figure standing on it") {
  EditorDocument document;
  document.player_starts.push_back(makeEditorPlayerStart(1, {1.5f, 2.5f, 0}));
  document.player_starts.push_back(makeEditorPlayerStart(2, {3.5f, 2.5f, 0}));
  document.player_starts[1].id = "start_02";
  document.player_starts[1].character = "mesh:hero";

  const std::vector<EditorCharacterFigure> figures =
      editorStartFigures(document);

  REQUIRE(figures.size() == 1);
  REQUIRE(figures[0].key == "player_start:start_02");
  REQUIRE(figures[0].character == "mesh:hero");
  REQUIRE(figures[0].feet.x == 3.5f);
  // Facing the way a player spawns aiming, and standing still.
  REQUIRE(figures[0].aim.x == 1.0f);
  REQUIRE(figures[0].aim.y == 0.0f);
  REQUIRE(figures[0].gait == EditorCharacterGait::STILL);
}
