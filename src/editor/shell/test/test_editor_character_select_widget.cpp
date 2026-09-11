#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-character-select-widget.h>
#include <engine/gui/gui-mouse-event.h>
#include <vector>

using namespace eng;
using namespace eng::editor;

namespace {

/// A selector over a 900×600 viewport showing three cards, recording what
/// it reports.
struct SelectFixture {
  EditorCharacterSelectWidget select;
  std::vector<size_t> chosen;
  int cancelled = 0;

  SelectFixture() {
    select.rect = makeRect(0.0f, 0.0f, 900.0f, 600.0f);
    select.on_chosen = [this](size_t index) {
      chosen.push_back(index);
    };
    select.on_cancelled = [this] {
      ++cancelled;
    };
    select.open({{"Scout"}, {"Tank"}, {"Medic"}}, 1);
  }

  bool press(float x, float y) {
    GuiMouseEvent event;
    event.button = GuiMouseButton::LEFT;
    event.x = x;
    event.y = y;
    return select.handleMouseDown(event);
  }
};

float midX(const Rect& r) {
  return r.x + r.w * 0.5f;
}
float midY(const Rect& r) {
  return r.y + r.h * 0.5f;
}

}  // namespace

TEST_CASE("the selector opens on the card it is given, and shows") {
  SelectFixture fixture;
  REQUIRE(fixture.select.isOpen());
  REQUIRE(fixture.select.visible);
  REQUIRE(fixture.select.highlighted() == 1);
}

TEST_CASE("clicking a card picks it") {
  SelectFixture fixture;
  const Rect card = characterCardRect(fixture.select.layout(), 2);

  REQUIRE_FALSE(fixture.press(midX(card), midY(card)));

  REQUIRE(fixture.chosen == std::vector<size_t>{2});
  REQUIRE(fixture.cancelled == 0);
}

TEST_CASE("the highlight wraps, and Enter picks what it is on") {
  SelectFixture fixture;
  fixture.select.moveHighlight(1);
  fixture.select.moveHighlight(1);
  REQUIRE(fixture.select.highlighted() == 0);
  fixture.select.moveHighlight(-1);

  fixture.select.confirm();

  REQUIRE(fixture.chosen == std::vector<size_t>{2});
}

TEST_CASE(
    "a click off the panel cancels, and one on it between cards does not") {
  SelectFixture fixture;
  const EditorCharacterSelectLayout parts = fixture.select.layout();

  (void)fixture.press(midX(parts.title), midY(parts.title));
  REQUIRE(fixture.cancelled == 0);
  (void)fixture.press(2.0f, 2.0f);
  REQUIRE(fixture.cancelled == 1);
  REQUIRE(fixture.chosen.empty());
}

TEST_CASE("a closed selector is hidden and takes nothing") {
  SelectFixture fixture;
  fixture.select.close();
  REQUIRE_FALSE(fixture.select.visible);
  const Rect card = characterCardRect(fixture.select.layout(), 0);
  (void)fixture.press(midX(card), midY(card));
  fixture.select.confirm();
  REQUIRE(fixture.chosen.empty());
}

TEST_CASE("cards sit in rows the viewport is wide enough for, centred") {
  const EditorCharacterSelectLayout wide =
      layoutEditorCharacterSelect(makeRect(0.0f, 0.0f, 1000.0f, 800.0f), 3);
  const EditorCharacterSelectLayout narrow =
      layoutEditorCharacterSelect(makeRect(0.0f, 0.0f, 320.0f, 800.0f), 3);

  REQUIRE(wide.columns == 3);
  REQUIRE(narrow.columns == 2);
  REQUIRE(wide.panel.x + wide.panel.w * 0.5f == 500.0f);
  // The third card wraps onto a second row.
  REQUIRE(characterCardRect(narrow, 2).y > characterCardRect(narrow, 0).y);
  REQUIRE(hitTestCharacterCard(wide, 3, 1.0f, 1.0f) == -1);
}
