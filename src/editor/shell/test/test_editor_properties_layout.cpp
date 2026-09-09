#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-properties-layout.h>
#include <editor/shell/editor-property-field.h>

using Catch::Approx;
using namespace eng::editor;

namespace {

eng::Rect panelRect() {
  return eng::makeRect(1000.0f, 64.0f, PROPERTIES_PANEL_WIDTH, 500.0f);
}

}  // namespace

TEST_CASE("the panel divides into a header, an asset line, and a body") {
  const EditorPropertiesLayout layout = layoutEditorProperties(panelRect());

  REQUIRE(layout.header.y == Approx(64.0f));
  REQUIRE(layout.header.h == Approx(PROPERTIES_HEADER_HEIGHT));
  REQUIRE(layout.asset.y == Approx(layout.header.y + layout.header.h));
  REQUIRE(layout.body.y >= layout.asset.y + layout.asset.h);
  // The header spans the panel; the body is inset so rows do not touch the
  // border.
  REQUIRE(layout.header.w == Approx(PROPERTIES_PANEL_WIDTH));
  REQUIRE(layout.body.w < layout.header.w);
}

TEST_CASE("a panel too short for its regions hands out no negative space") {
  const EditorPropertiesLayout layout =
      layoutEditorProperties(eng::makeRect(0.0f, 0.0f, 200.0f, 10.0f));
  REQUIRE(layout.header.h == Approx(10.0f));
  REQUIRE(layout.asset.h == Approx(0.0f));
  REQUIRE(layout.body.h == Approx(0.0f));
}

TEST_CASE("rows stack down the body without overlapping") {
  const eng::Rect body = layoutEditorProperties(panelRect()).body;
  for (size_t i = 1; i < EDITOR_PLACEMENT_FIELD_COUNT; ++i) {
    const eng::Rect above = propertyRowRect(body, i - 1);
    const eng::Rect row = propertyRowRect(body, i);
    REQUIRE(row.y >= above.y + above.h);
  }
}

TEST_CASE("a row is a label, two step buttons, and the value between them") {
  const eng::Rect body = layoutEditorProperties(panelRect()).body;
  const eng::Rect row = propertyRowRect(body, 0);
  const eng::Rect label = propertyLabelRect(row);
  const eng::Rect decrement = propertyDecrementRect(row);
  const eng::Rect value = propertyValueRect(row);
  const eng::Rect increment = propertyIncrementRect(row);

  REQUIRE(label.x == Approx(row.x));
  REQUIRE(decrement.x == Approx(label.x + label.w));
  REQUIRE(value.x == Approx(decrement.x + decrement.w));
  REQUIRE(increment.x == Approx(value.x + value.w));
  REQUIRE(increment.x + increment.w == Approx(row.x + row.w));
  // The value box is the largest of the three controls: it is the one
  // holding a number and the one a drag has to land on.
  REQUIRE(value.w > decrement.w);
}

TEST_CASE("a point in a row finds that row") {
  const eng::Rect body = layoutEditorProperties(panelRect()).body;
  for (size_t i = 0; i < EDITOR_PLACEMENT_FIELD_COUNT; ++i) {
    const eng::Rect row = propertyRowRect(body, i);
    REQUIRE(hitTestPropertyRow(body, EDITOR_PLACEMENT_FIELD_COUNT, row.x + 1.0f,
                               row.y + 1.0f) == static_cast<int>(i));
  }
}

TEST_CASE("a point in the gap between rows finds nothing") {
  // The gap belongs to neither row. Rounding a press to the nearer one
  // would step a value the pointer was not over.
  const eng::Rect body = layoutEditorProperties(panelRect()).body;
  const eng::Rect first = propertyRowRect(body, 0);
  REQUIRE(hitTestPropertyRow(body, EDITOR_PLACEMENT_FIELD_COUNT, first.x + 1.0f,
                             first.y + first.h + PROPERTIES_ROW_GAP * 0.5f) ==
          -1);
}

TEST_CASE("a point past the last row finds nothing") {
  const eng::Rect body = layoutEditorProperties(panelRect()).body;
  const eng::Rect last =
      propertyRowRect(body, EDITOR_PLACEMENT_FIELD_COUNT - 1);
  REQUIRE(hitTestPropertyRow(body, EDITOR_PLACEMENT_FIELD_COUNT, last.x + 1.0f,
                             last.y + last.h + 20.0f) == -1);
}
