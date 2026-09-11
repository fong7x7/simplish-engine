#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-properties-layout.h>
#include <editor/shell/editor-properties-widget.h>
#include <editor/shell/editor-property-ops.h>
#include <editor/shell/editor-property-traits.h>
#include <editor/shell/editor-scale-slider.h>
#include <iterator>
#include <vector>

using Catch::Approx;
using namespace eng::editor;

namespace {

/// One reported change.
struct Change {
  EditorPropertyField field = EditorPropertyField::POSITION_X;
  float value = 0.0f;
  EditorPropertyEdit edit = EditorPropertyEdit::PREVIEW;
};

/// A panel showing one placement, recording everything it reports.
struct PanelFixture {
  EditorPropertiesWidget panel;
  std::vector<Change> changes;

  PanelFixture() {
    panel.rect = eng::makeRect(1000.0f, 64.0f, PROPERTIES_PANEL_WIDTH, 500.0f);
    panel.on_property_changed = [this](EditorPropertyField field, float value,
                                       EditorPropertyEdit edit) {
      changes.push_back({field, value, edit});
    };
    panel.setSelection("crate", EditorPlacement{});
  }

  /// Press at a point, with the left button.
  bool press(float x, float y) {
    eng::GuiMouseEvent event{};
    event.x = x;
    event.y = y;
    event.button = eng::GuiMouseButton::LEFT;
    return panel.handleMouseDown(event);
  }

  /// Move to a point mid-drag.
  void moveTo(float x, float y) {
    eng::GuiMouseEvent event{};
    event.x = x;
    event.y = y;
    panel.handleMouseMove(event);
  }

  /// Release at a point.
  void release(float x, float y) {
    eng::GuiMouseEvent event{};
    event.x = x;
    event.y = y;
    event.button = eng::GuiMouseButton::LEFT;
    panel.handleMouseUp(event);
  }

  /// How many finished edits were reported, which is how many entries the
  /// history would grow by.
  [[nodiscard]] size_t commits() const {
    size_t count = 0;
    for (const Change& change : changes) {
      count += change.edit == EditorPropertyEdit::COMMIT ? 1 : 0;
    }
    return count;
  }

  /// A point inside a row's control.
  [[nodiscard]] eng::Rect rowOf(EditorPropertyField field) const {
    return panel.fieldRowRect(field);
  }
};

/// Centre of a rect, where a press is unambiguous.
float midX(const eng::Rect& rect) {
  return rect.x + rect.w * 0.5f;
}
float midY(const eng::Rect& rect) {
  return rect.y + rect.h * 0.5f;
}

}  // namespace

TEST_CASE("a panel with nothing selected is hidden and takes no width") {
  EditorPropertiesWidget panel;
  REQUIRE_FALSE(panel.hasSelection());
  REQUIRE_FALSE(panel.visible);
  REQUIRE(panel.preferredWidth() == Approx(0.0f));
}

TEST_CASE("selecting a placement shows the panel and takes its width") {
  PanelFixture fixture;
  REQUIRE(fixture.panel.hasSelection());
  REQUIRE(fixture.panel.visible);
  REQUIRE(fixture.panel.preferredWidth() == Approx(PROPERTIES_PANEL_WIDTH));
}

TEST_CASE("clearing the selection puts the panel away") {
  PanelFixture fixture;
  fixture.panel.clearSelection();
  REQUIRE_FALSE(fixture.panel.hasSelection());
  REQUIRE_FALSE(fixture.panel.visible);
  REQUIRE(fixture.panel.preferredWidth() == Approx(0.0f));
}

TEST_CASE("a step button moves a value by one step and commits it") {
  PanelFixture fixture;
  const eng::Rect row = fixture.rowOf(EditorPropertyField::POSITION_X);
  const eng::Rect increment = propertyIncrementRect(row);

  REQUIRE_FALSE(fixture.press(midX(increment), midY(increment)));
  REQUIRE(fixture.changes.size() == 1);
  REQUIRE(fixture.changes[0].field == EditorPropertyField::POSITION_X);
  REQUIRE(fixture.changes[0].value == Approx(EDITOR_POSITION_STEP));
  // One click is one finished edit, so it is one entry in the history.
  REQUIRE(fixture.changes[0].edit == EditorPropertyEdit::COMMIT);
}

TEST_CASE("the other step button moves the value the other way") {
  PanelFixture fixture;
  const eng::Rect row = fixture.rowOf(EditorPropertyField::POSITION_Y);
  const eng::Rect decrement = propertyDecrementRect(row);

  fixture.press(midX(decrement), midY(decrement));
  REQUIRE(fixture.changes.size() == 1);
  REQUIRE(fixture.changes[0].field == EditorPropertyField::POSITION_Y);
  REQUIRE(fixture.changes[0].value == Approx(-EDITOR_POSITION_STEP));
}

TEST_CASE("a rotation steps in degrees, not tiles") {
  PanelFixture fixture;
  const eng::Rect row = fixture.rowOf(EditorPropertyField::ROTATION_Z);
  const eng::Rect increment = propertyIncrementRect(row);

  fixture.press(midX(increment), midY(increment));
  REQUIRE(fixture.changes[0].field == EditorPropertyField::ROTATION_Z);
  REQUIRE(fixture.changes[0].value == Approx(EDITOR_ROTATION_STEP));
}

TEST_CASE("every property has a row that reaches its own field") {
  PanelFixture fixture;
  for (EditorPropertyField field : EDITOR_PLACEMENT_FIELDS) {
    fixture.changes.clear();
    const eng::Rect increment = propertyIncrementRect(fixture.rowOf(field));
    fixture.press(midX(increment), midY(increment));
    REQUIRE(fixture.changes.size() == 1);
    REQUIRE(fixture.changes[0].field == field);
  }
}

TEST_CASE("a drag on the value box scrubs it and captures the pointer") {
  PanelFixture fixture;
  const eng::Rect value =
      propertyValueRect(fixture.rowOf(EditorPropertyField::POSITION_X));
  const float start_x = midX(value);
  const float y = midY(value);

  REQUIRE(fixture.press(start_x, y));
  REQUIRE(fixture.panel.dragging());
  fixture.moveTo(start_x + 64.0f, y);

  REQUIRE(fixture.changes.size() == 1);
  REQUIRE(fixture.changes[0].edit == EditorPropertyEdit::PREVIEW);
  REQUIRE(fixture.changes[0].value ==
          Approx(64.0f * EDITOR_POSITION_DRAG_PER_PIXEL));
}

TEST_CASE("a drag reports every value against where it began") {
  PanelFixture fixture;
  const eng::Rect value =
      propertyValueRect(fixture.rowOf(EditorPropertyField::POSITION_X));
  const float start_x = midX(value);
  const float y = midY(value);

  fixture.press(start_x, y);
  fixture.moveTo(start_x + 32.0f, y);
  fixture.moveTo(start_x + 64.0f, y);
  // Returning to where it started returns the value it started with, which
  // a run of deltas would not: each would round on its own.
  fixture.moveTo(start_x, y);

  REQUIRE(fixture.changes.back().value == Approx(0.0f));
}

TEST_CASE("a drag is one committed edit, however far it travelled") {
  PanelFixture fixture;
  const eng::Rect value =
      propertyValueRect(fixture.rowOf(EditorPropertyField::POSITION_Z));
  const float start_x = midX(value);
  const float y = midY(value);

  fixture.press(start_x, y);
  fixture.moveTo(start_x + 10.0f, y);
  fixture.moveTo(start_x + 20.0f, y);
  fixture.release(start_x + 20.0f, y);

  REQUIRE_FALSE(fixture.panel.dragging());
  REQUIRE(fixture.commits() == 1);
  REQUIRE(fixture.changes.back().edit == EditorPropertyEdit::COMMIT);
  REQUIRE(fixture.changes.back().value ==
          Approx(20.0f * EDITOR_POSITION_DRAG_PER_PIXEL));
}

TEST_CASE("a drag released outside the panel still commits") {
  PanelFixture fixture;
  const eng::Rect value =
      propertyValueRect(fixture.rowOf(EditorPropertyField::POSITION_X));

  fixture.press(midX(value), midY(value));
  fixture.moveTo(midX(value) - 400.0f, midY(value));
  // The pointer left the panel, which does not undo the edit it made on the
  // way out.
  fixture.release(midX(value) - 400.0f, midY(value));
  REQUIRE(fixture.changes.back().edit == EditorPropertyEdit::COMMIT);
}

TEST_CASE("a rotation dragged past half a turn wraps as it is reported") {
  PanelFixture fixture;
  const eng::Rect value =
      propertyValueRect(fixture.rowOf(EditorPropertyField::ROTATION_Z));

  fixture.press(midX(value), midY(value));
  fixture.moveTo(midX(value) + 270.0f, midY(value));
  // What the panel reports is what the placement will hold, wrap included,
  // so the number on screen and the number in the document never differ.
  REQUIRE(fixture.changes.back().value == Approx(-90.0f));
}

TEST_CASE("a press in the gap between rows changes nothing") {
  PanelFixture fixture;
  const eng::Rect row = fixture.rowOf(EditorPropertyField::POSITION_X);

  REQUIRE_FALSE(
      fixture.press(midX(row), row.y + row.h + PROPERTIES_ROW_GAP * 0.5f));
  REQUIRE(fixture.changes.empty());
}

TEST_CASE("a press on the label changes nothing") {
  PanelFixture fixture;
  const eng::Rect label =
      propertyLabelRect(fixture.rowOf(EditorPropertyField::POSITION_X));
  REQUIRE_FALSE(fixture.press(midX(label), midY(label)));
  REQUIRE(fixture.changes.empty());
}

TEST_CASE("a panel with nothing selected ignores presses") {
  EditorPropertiesWidget panel;
  panel.rect = eng::makeRect(0.0f, 0.0f, PROPERTIES_PANEL_WIDTH, 500.0f);
  bool reported = false;
  panel.on_property_changed = [&reported](EditorPropertyField, float,
                                          EditorPropertyEdit) {
    reported = true;
  };
  eng::GuiMouseEvent event{};
  event.button = eng::GuiMouseButton::LEFT;
  event.x = 40.0f;
  event.y = 100.0f;
  REQUIRE_FALSE(panel.handleMouseDown(event));
  REQUIRE_FALSE(reported);
}

TEST_CASE("losing the selection mid-drag ends the drag") {
  PanelFixture fixture;
  const eng::Rect value =
      propertyValueRect(fixture.rowOf(EditorPropertyField::POSITION_X));
  fixture.press(midX(value), midY(value));

  // The placement the drag was editing is gone — undone, or the project
  // closed. There is nothing left to commit the value to.
  fixture.panel.clearSelection();
  REQUIRE_FALSE(fixture.panel.dragging());
  const size_t before = fixture.changes.size();
  fixture.release(midX(value), midY(value));
  REQUIRE(fixture.changes.size() == before);
}

TEST_CASE("the panel shows the values it is given") {
  PanelFixture fixture;
  EditorPlacement placement;
  placement.position = {2.0f, 3.0f, 1.0f};
  placement.rotation = {0.0f, 0.0f, 45.0f};
  fixture.panel.setSelection("lamp", placement);

  REQUIRE(fixture.panel.value(EditorPropertyField::POSITION_Y) == Approx(3.0f));
  REQUIRE(fixture.panel.value(EditorPropertyField::ROTATION_Z) ==
          Approx(45.0f));
}

TEST_CASE("selecting a light lists the properties its kind uses") {
  PanelFixture fixture;
  fixture.panel.setSelection(
      "Point Light",
      makeEditorLight(EditorLightKind::POINT, {1.0f, 2.0f, 3.0f}));

  REQUIRE(fixture.panel.fields().size() ==
          std::size(EDITOR_POINT_LIGHT_FIELDS));
  REQUIRE(fixture.panel.value(EditorPropertyField::POSITION_Z) == Approx(3.0f));
  // A directional light's rows are not this light's rows, and a placement's
  // are nobody's but a placement's.
  REQUIRE(fixture.rowOf(EditorPropertyField::ROTATION_X).w == 0.0f);
}

TEST_CASE("a directional light has a direction where a point light has a "
          "position") {
  PanelFixture fixture;
  fixture.panel.setSelection(
      "Directional Light",
      makeEditorLight(EditorLightKind::DIRECTIONAL, {1.0f, 2.0f, 3.0f}));

  REQUIRE(fixture.rowOf(EditorPropertyField::POSITION_X).w == 0.0f);
  const eng::Rect increment =
      propertyIncrementRect(fixture.rowOf(EditorPropertyField::DIRECTION_X));
  fixture.press(midX(increment), midY(increment));
  REQUIRE(fixture.changes.size() == 1);
  REQUIRE(fixture.changes[0].field == EditorPropertyField::DIRECTION_X);
}

TEST_CASE("a light's intensity never goes below nothing") {
  PanelFixture fixture;
  fixture.panel.setSelection(
      "Point Light", makeEditorLight(EditorLightKind::POINT, {0.0f, 0.0f}));
  const eng::Rect value =
      propertyValueRect(fixture.rowOf(EditorPropertyField::INTENSITY));

  fixture.press(midX(value), midY(value));
  // Far enough left to take it past zero, which is not a brightness.
  fixture.moveTo(midX(value) - 600.0f, midY(value));
  REQUIRE(fixture.changes.back().value == Approx(0.0f));
}

TEST_CASE("the panel shows the reference for a selected placement") {
  PanelFixture fixture;
  EditorPlacement placement;
  placement.id = "crate_01";
  fixture.panel.setSelection("crate", placement);

  // What a level or logic file writes to name this crate, shown qualified
  // so it can be copied verbatim rather than assembled by hand.
  REQUIRE(fixture.panel.reference() == "prop:crate_01");
}

TEST_CASE("the panel shows the reference for a selected light") {
  PanelFixture fixture;
  EditorLight light = makeEditorLight(EditorLightKind::POINT, {});
  light.id = "point_02";
  fixture.panel.setSelection("Point Light", light);

  REQUIRE(fixture.panel.reference() == "light:point_02");
}

TEST_CASE("clearing the selection clears the reference with it") {
  PanelFixture fixture;
  EditorPlacement placement;
  placement.id = "crate_01";
  fixture.panel.setSelection("crate", placement);
  fixture.panel.clearSelection();
  REQUIRE(fixture.panel.reference().empty());
}

TEST_CASE("a player start lists its player first, then where it stands") {
  PanelFixture fixture;
  EditorPlayerStart start = makeEditorPlayerStart(2, {1.5f, 2.5f, 0.0f});
  start.id = "start_01";

  fixture.panel.setSelection(editorPlayerStartName(start), start);

  REQUIRE(fixture.panel.fields() == std::vector<EditorPropertyField>(
                                        std::begin(EDITOR_PLAYER_START_FIELDS),
                                        std::end(EDITOR_PLAYER_START_FIELDS)));
  REQUIRE(fixture.panel.value(EditorPropertyField::PLAYER) == Approx(2.0f));
  REQUIRE(fixture.panel.value(EditorPropertyField::POSITION_Y) == Approx(2.5f));
  REQUIRE(fixture.panel.reference() == "player_start:start_01");
}

TEST_CASE("stepping a start's player moves to the next player and stops at 4") {
  PanelFixture fixture;
  fixture.panel.setSelection("Player 4 Start", makeEditorPlayerStart(4, {}));
  const eng::Rect row = fixture.rowOf(EditorPropertyField::PLAYER);
  const eng::Rect increment = propertyIncrementRect(row);
  const eng::Rect decrement = propertyDecrementRect(row);

  (void)fixture.press(midX(increment), midY(increment));
  REQUIRE(fixture.changes.back().value == Approx(4.0f));
  (void)fixture.press(midX(decrement), midY(decrement));
  REQUIRE(fixture.changes.back().value == Approx(3.0f));
  REQUIRE(fixture.changes.back().edit == EditorPropertyEdit::COMMIT);
}

TEST_CASE("a placement lists Collides as its last row, ticked") {
  PanelFixture fixture;
  REQUIRE(fixture.panel.fields().back() == EditorPropertyField::COLLIDES);
  REQUIRE(fixture.panel.value(EditorPropertyField::COLLIDES) == Approx(1.0f));
}

TEST_CASE("clicking anywhere on the Collides row flips it and commits") {
  PanelFixture fixture;
  const eng::Rect row = fixture.rowOf(EditorPropertyField::COLLIDES);

  // On the label: a checkbox's label is part of what can be clicked.
  REQUIRE_FALSE(fixture.press(row.x + 2.0f, midY(row)));
  REQUIRE(fixture.changes.size() == 1);
  REQUIRE(fixture.changes[0].field == EditorPropertyField::COLLIDES);
  REQUIRE(fixture.changes[0].value == Approx(0.0f));
  REQUIRE(fixture.changes[0].edit == EditorPropertyEdit::COMMIT);

  // On the box: back on again.
  const eng::Rect box = propertyCheckboxRect(row);
  REQUIRE_FALSE(fixture.press(midX(box), midY(box)));
  REQUIRE(fixture.changes.back().value == Approx(1.0f));
  REQUIRE_FALSE(fixture.panel.dragging());
}

namespace {

/// A panel showing a rigged placement with three clips, recording every
/// choice it reports.
struct ClipFixture : PanelFixture {
  std::vector<size_t> picked;
  std::vector<EditorChoiceKind> rows;

  ClipFixture() {
    panel.addChoices(EditorChoiceKind::ANIMATION, {"idle", "walk", "run"}, 0);
    panel.on_choice_changed = [this](EditorChoiceKind kind, size_t index) {
      rows.push_back(kind);
      picked.push_back(index);
    };
  }
};

constexpr EditorChoiceKind CLIPS = EditorChoiceKind::ANIMATION;

}  // namespace

TEST_CASE("a rigged placement lists an Animation row under its properties") {
  ClipFixture fixture;
  const eng::Rect row = fixture.panel.choiceRowRect(CLIPS);
  REQUIRE(row.h > 0.0f);
  REQUIRE(row.y > fixture.rowOf(EditorPropertyField::COLLIDES).y);
  REQUIRE(fixture.panel.hasChoiceRow(CLIPS));
  REQUIRE(fixture.panel.choice(CLIPS) == "idle");
}

TEST_CASE("the choice row's buttons step through the names, wrapping") {
  ClipFixture fixture;
  const eng::Rect row = fixture.panel.choiceRowRect(CLIPS);
  const eng::Rect next = propertyIncrementRect(row);
  const eng::Rect back = propertyDecrementRect(row);

  REQUIRE_FALSE(fixture.press(midX(next), midY(next)));
  REQUIRE_FALSE(fixture.press(midX(back), midY(back)));
  REQUIRE_FALSE(fixture.press(midX(back), midY(back)));
  REQUIRE(fixture.picked == std::vector<size_t>{1, 0, 2});
  REQUIRE(fixture.panel.choice(CLIPS) == "run");
  REQUIRE(fixture.panel.choiceIndex(CLIPS) == 2);
  // A choice is a name, not a property value: nothing numeric was reported.
  REQUIRE(fixture.changes.empty());
}

TEST_CASE("a press on the choice row's name changes nothing") {
  ClipFixture fixture;
  const eng::Rect value = propertyValueRect(fixture.panel.choiceRowRect(CLIPS));
  REQUIRE_FALSE(fixture.press(midX(value), midY(value)));
  REQUIRE(fixture.picked.empty());
  REQUIRE_FALSE(fixture.panel.dragging());
}

TEST_CASE("the choice row shows the name it is given") {
  ClipFixture fixture;
  fixture.panel.addChoices(CLIPS, {"idle", "walk", "run"}, 1);
  REQUIRE(fixture.panel.choice(CLIPS) == "walk");
  // Out of range is the first, rather than past the end.
  fixture.panel.addChoices(CLIPS, {"idle", "walk", "run"}, 9);
  REQUIRE(fixture.panel.choice(CLIPS) == "idle");
  // Offered again, it replaced its row rather than adding another.
  REQUIRE(fixture.panel.choiceRows().size() == 1);
}

TEST_CASE("a player start lists a Character row after its properties") {
  PanelFixture fixture;
  fixture.panel.setSelection("Player 1 Start", EditorPlayerStart{});
  fixture.panel.addChoices(EditorChoiceKind::CHARACTER, {"Stand-in", "hero"},
                           1);
  REQUIRE(fixture.panel.choice(EditorChoiceKind::CHARACTER) == "hero");
  REQUIRE(fixture.panel.choiceRowRect(EditorChoiceKind::CHARACTER).y >
          fixture.rowOf(fixture.panel.fields().back()).y);
}

TEST_CASE("choice rows stack in the order they are offered, each its own") {
  ClipFixture fixture;
  fixture.panel.addChoices(EditorChoiceKind::BEHAVIOR, {"None", "Guard"}, 1);
  fixture.panel.addChoices(EditorChoiceKind::FACTION,
                           {"Hostile", "Neutral", "Friendly"}, 0);
  const eng::Rect behavior =
      fixture.panel.choiceRowRect(EditorChoiceKind::BEHAVIOR);
  const eng::Rect faction =
      fixture.panel.choiceRowRect(EditorChoiceKind::FACTION);
  REQUIRE(behavior.y > fixture.panel.choiceRowRect(CLIPS).y);
  REQUIRE(faction.y > behavior.y);

  const eng::Rect next = propertyIncrementRect(faction);
  REQUIRE_FALSE(fixture.press(midX(next), midY(next)));
  REQUIRE(fixture.rows == std::vector{EditorChoiceKind::FACTION});
  REQUIRE(fixture.panel.choice(EditorChoiceKind::FACTION) == "Neutral");
  REQUIRE(fixture.panel.choice(EditorChoiceKind::BEHAVIOR) == "Guard");
}

TEST_CASE("an empty list takes a choice row away") {
  ClipFixture fixture;
  fixture.panel.addChoices(CLIPS, {}, 0);
  REQUIRE_FALSE(fixture.panel.hasChoiceRow(CLIPS));
}

TEST_CASE("a new selection drops the previous one's choices") {
  ClipFixture fixture;
  fixture.panel.setSelection("crate", EditorPlacement{});
  REQUIRE(fixture.panel.choiceRowRect(CLIPS).h == 0.0f);
  REQUIRE(fixture.panel.choice(CLIPS).empty());
  REQUIRE(fixture.panel.choiceRows().empty());
}

namespace {

/// The track of the scale slider in @p fixture's panel.
eng::Rect scaleTrack(const PanelFixture& fixture) {
  return propertyValueRect(fixture.rowOf(EditorPropertyField::SCALE));
}

}  // namespace

TEST_CASE("a placement's panel lists a scale row") {
  const PanelFixture fixture;
  const eng::Rect row = fixture.rowOf(EditorPropertyField::SCALE);
  REQUIRE(row.w > 0.0f);
  REQUIRE(fixture.panel.value(EditorPropertyField::SCALE) == Approx(1.0f));
}

TEST_CASE("pressing the slider jumps the scale to that point on it") {
  PanelFixture fixture;
  const eng::Rect track = scaleTrack(fixture);

  // Three quarters along is a doubling and a half up from 1, the middle.
  REQUIRE(fixture.press(track.x + track.w * 0.75f, midY(track)));
  REQUIRE(fixture.panel.dragging());
  REQUIRE(fixture.changes.size() == 1);
  REQUIRE(fixture.changes[0].field == EditorPropertyField::SCALE);
  REQUIRE(fixture.changes[0].edit == EditorPropertyEdit::PREVIEW);
  REQUIRE(fixture.changes[0].value ==
          Approx(editorScaleFromSliderFraction(0.75f)));
}

TEST_CASE("dragging the slider follows the pointer, not the distance moved") {
  // A value box scrubs by how far the pointer has travelled; a slider is
  // wherever the pointer is. Pressing at one point and moving to another
  // gives that other point's value, whatever the press gave.
  PanelFixture fixture;
  const eng::Rect track = scaleTrack(fixture);
  fixture.press(track.x + track.w * 0.25f, midY(track));
  fixture.moveTo(track.x + track.w * 0.5f, midY(track));
  REQUIRE(fixture.changes.back().value == Approx(1.0f));
}

TEST_CASE("a slider drag is one committed edit") {
  PanelFixture fixture;
  const eng::Rect track = scaleTrack(fixture);
  fixture.press(track.x + track.w * 0.6f, midY(track));
  fixture.moveTo(track.x + track.w * 0.7f, midY(track));
  fixture.moveTo(track.x + track.w * 0.8f, midY(track));
  fixture.release(track.x + track.w * 0.8f, midY(track));

  REQUIRE(fixture.commits() == 1);
  REQUIRE(fixture.changes.back().edit == EditorPropertyEdit::COMMIT);
  REQUIRE(fixture.changes.back().value ==
          Approx(editorScaleFromSliderFraction(0.8f)));
}

TEST_CASE("dragging past the slider's end holds at the largest scale") {
  PanelFixture fixture;
  const eng::Rect track = scaleTrack(fixture);
  fixture.press(midX(track), midY(track));
  fixture.moveTo(track.x + track.w + 300.0f, midY(track));
  REQUIRE(fixture.changes.back().value == Approx(EDITOR_SCALE_MAX));
}

TEST_CASE("the scale's step buttons move it between fixed stops") {
  PanelFixture fixture;
  const eng::Rect row = fixture.rowOf(EditorPropertyField::SCALE);
  const eng::Rect up = propertyIncrementRect(row);

  REQUIRE_FALSE(fixture.press(midX(up), midY(up)));
  REQUIRE(fixture.changes.size() == 1);
  REQUIRE(fixture.changes[0].edit == EditorPropertyEdit::COMMIT);
  REQUIRE(fixture.changes[0].value == Approx(editorScaleStepped(1.0f, 1)));
}

TEST_CASE("a step down from near one lands exactly on one") {
  // What the step buttons are there for: a slider can almost never be let
  // go of on exactly 1, and a stop is a fixed point it can be stepped to.
  PanelFixture fixture;
  EditorPlacement placement;
  placement.scale = 1.04f;
  fixture.panel.setSelection("crate", placement);
  const eng::Rect down =
      propertyDecrementRect(fixture.rowOf(EditorPropertyField::SCALE));

  fixture.press(midX(down), midY(down));
  REQUIRE(fixture.changes.back().value == 1.0f);
}
