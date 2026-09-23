#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-sound-widget.h>
#include <optional>
#include <utility>

using namespace eng;
using namespace eng::editor;

namespace {

/// The rows the editor gives the screen, less their values.
std::vector<EditorSoundRow> someRows() {
  return {{.name = "Volume"},
          {.kind = EditorSoundRowKind::VOLUME,
           .name = "Master",
           .key = "master",
           .level = 0.5f},
          {.kind = EditorSoundRowKind::MUTE, .name = "Mute"},
          {.name = "Project sounds"},
          {.kind = EditorSoundRowKind::SLOT,
           .name = "Blast",
           .key = "combat.blast"}};
}

/// A screen open on @p rows over an 800 × 600 viewport.
EditorSoundWidget openOn(std::vector<EditorSoundRow> rows) {
  EditorSoundWidget screen;
  screen.rect = makeRect(0.0f, 0.0f, 800.0f, 600.0f);
  screen.open(std::move(rows));
  return screen;
}

GuiMouseEvent clickAt(float x, float y) {
  GuiMouseEvent event;
  event.x = x;
  event.y = y;
  event.button = GuiMouseButton::LEFT;
  return event;
}

}  // namespace

TEST_CASE("the screen opens on the first row that is not a heading") {
  EditorSoundWidget screen = openOn(someRows());
  REQUIRE(screen.isOpen());
  REQUIRE(screen.visible);
  REQUIRE(screen.highlighted() == 1);
  screen.close();
  REQUIRE_FALSE(screen.isOpen());
  REQUIRE_FALSE(screen.visible);
}

TEST_CASE("the highlight steps over headings, and wraps") {
  EditorSoundWidget screen = openOn(someRows());
  screen.moveHighlight(1);
  REQUIRE(screen.highlighted() == 2);
  screen.moveHighlight(1);
  REQUIRE(screen.highlighted() == 4);
  screen.moveHighlight(1);
  REQUIRE(screen.highlighted() == 1);
  screen.moveHighlight(-1);
  REQUIRE(screen.highlighted() == 4);
}

TEST_CASE("refreshing keeps the highlight where it was") {
  EditorSoundWidget screen = openOn(someRows());
  screen.moveHighlight(2);
  screen.refresh(someRows());
  REQUIRE(screen.highlighted() == 4);
}

TEST_CASE("a click on a volume's bar reports where along it") {
  EditorSoundWidget screen = openOn(someRows());
  std::optional<std::pair<size_t, float>> picked;
  screen.on_level_picked = [&picked](size_t row, float level) {
    picked = {row, level};
  };
  const Rect bar = screen.barRect(1);
  (void)screen.handleMouseDown(
      clickAt(bar.x + bar.w * 0.25f, bar.y + bar.h * 0.5f));
  REQUIRE(picked.has_value());
  REQUIRE(picked->first == 1);
  REQUIRE(picked->second == 0.25f);
}

TEST_CASE("a click on a row highlights it; one off the panel closes") {
  EditorSoundWidget screen = openOn(someRows());
  bool dismissed = false;
  screen.on_dismissed = [&dismissed] {
    dismissed = true;
  };
  const Rect slot = screen.rowRect(4);
  (void)screen.handleMouseDown(clickAt(slot.x + 4.0f, slot.y + 4.0f));
  REQUIRE(screen.highlighted() == 4);
  (void)screen.handleMouseDown(clickAt(2.0f, 2.0f));
  REQUIRE(dismissed);
}
