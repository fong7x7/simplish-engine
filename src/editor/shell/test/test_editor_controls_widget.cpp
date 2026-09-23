#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-controls-widget.h>
#include <engine/gui/gui-widget-tree.h>

using namespace eng;
using namespace eng::editor;

namespace {

/// Three rows: two moves and fire.
std::vector<EditorControlsRow> threeRows() {
  return {{input::InputAction::MOVE_UP, "Move up", "W", "D-pad Up"},
          {input::InputAction::MOVE_DOWN, "Move down", "S", "D-pad Down"},
          {input::InputAction::FIRE, "Fire", "—", "RT"}};
}

/// The screen, open over a 1000 by 700 viewport.
EditorControlsWidget openScreen() {
  EditorControlsWidget screen;
  screen.rect = {0.0f, 0.0f, 1000.0f, 700.0f};
  screen.open(threeRows());
  return screen;
}

}  // namespace

TEST_CASE("the controls screen opens browsing, on the first row") {
  EditorControlsWidget screen = openScreen();
  REQUIRE(screen.isOpen());
  REQUIRE(screen.visible);
  REQUIRE(screen.mode() == EditorControlsMode::BROWSING);
  REQUIRE(screen.highlighted() == 0);
}

TEST_CASE("the highlight wraps, and holds still while listening") {
  EditorControlsWidget screen = openScreen();
  screen.moveHighlight(-1);
  REQUIRE(screen.highlighted() == 2);
  screen.listen();
  screen.moveHighlight(1);
  REQUIRE(screen.highlighted() == 2);
  screen.stopListening();
  REQUIRE(screen.mode() == EditorControlsMode::BROWSING);
}

TEST_CASE("a click on a row listens for it; off the panel it closes") {
  EditorControlsWidget screen = openScreen();
  bool dismissed = false;
  screen.on_dismissed = [&dismissed] {
    dismissed = true;
  };
  const Rect row = screen.rowRect(1);
  (void)screen.handleMouseDown({.type = GuiMouseEventType::BUTTON_DOWN,
                                .x = row.x + 5.0f,
                                .y = row.y + 5.0f});
  REQUIRE(screen.highlighted() == 1);
  REQUIRE(screen.mode() == EditorControlsMode::LISTENING);
  (void)screen.handleMouseDown(
      {.type = GuiMouseEventType::BUTTON_DOWN, .x = 2.0f, .y = 2.0f});
  REQUIRE(dismissed);
}

TEST_CASE("fresh rows keep the highlight and stop listening") {
  EditorControlsWidget screen = openScreen();
  screen.moveHighlight(2);
  screen.listen();
  screen.refresh(threeRows());
  REQUIRE(screen.highlighted() == 2);
  REQUIRE(screen.mode() == EditorControlsMode::BROWSING);
  screen.close();
  REQUIRE_FALSE(screen.isOpen());
  REQUIRE_FALSE(screen.visible);
}
