#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-property-ops.h>
#include <editor/shell/editor-property-traits.h>

using Catch::Approx;
using namespace eng::editor;

TEST_CASE("every property reads back what was written to it") {
  EditorPlacement placement;
  float written = 1.0f;
  for (EditorPropertyField field : EDITOR_PLACEMENT_FIELDS) {
    // A toggle holds on or off, not a number; it has its own test below.
    if (editorPropertyFieldIsToggle(field)) {
      continue;
    }
    setEditorPropertyValue(placement, field, written);
    REQUIRE(editorPropertyValue(placement, field) == Approx(written));
    written += 1.0f;
  }
  // Six distinct numbers came back, so no two fields share storage.
  REQUIRE(placement.position.x == Approx(1.0f));
  REQUIRE(placement.position.z == Approx(3.0f));
  REQUIRE(placement.rotation.z == Approx(6.0f));
}

TEST_CASE("positions are written through untouched") {
  EditorPlacement placement;
  // A distance has no range to wrap into: a level is as wide as it is.
  setEditorPropertyValue(placement, EditorPropertyField::POSITION_X, 4000.5f);
  REQUIRE(placement.position.x == Approx(4000.5f));
  setEditorPropertyValue(placement, EditorPropertyField::POSITION_Y, -12.25f);
  REQUIRE(placement.position.y == Approx(-12.25f));
}

TEST_CASE("an angle past half a turn wraps to the short way round") {
  EditorPlacement placement;
  setEditorPropertyValue(placement, EditorPropertyField::ROTATION_Z, 270.0f);
  REQUIRE(placement.rotation.z == Approx(-90.0f));
}

TEST_CASE("an angle turned many times over stays in range") {
  EditorPlacement placement;
  // Dragging a rotation for long enough gets here, and a field showing
  // 3645 is a field nobody can read.
  setEditorPropertyValue(placement, EditorPropertyField::ROTATION_X, 3645.0f);
  REQUIRE(placement.rotation.x == Approx(45.0f));
  setEditorPropertyValue(placement, EditorPropertyField::ROTATION_Y, -3645.0f);
  REQUIRE(placement.rotation.y == Approx(-45.0f));
}

TEST_CASE("angles and distances step and drag at their own rates") {
  // Degrees and tiles are not the same size, and a step that suits one is
  // useless for the other.
  REQUIRE(editorPropertyStep(EditorPropertyField::POSITION_X) ==
          Approx(EDITOR_POSITION_STEP));
  REQUIRE(editorPropertyStep(EditorPropertyField::ROTATION_X) ==
          Approx(EDITOR_ROTATION_STEP));
  REQUIRE(editorPropertyDragPerPixel(EditorPropertyField::POSITION_Z) ==
          Approx(EDITOR_POSITION_DRAG_PER_PIXEL));
  REQUIRE(editorPropertyDragPerPixel(EditorPropertyField::ROTATION_Z) ==
          Approx(EDITOR_ROTATION_DRAG_PER_PIXEL));
}

TEST_CASE("values are written to a fixed number of places") {
  REQUIRE(formatEditorPropertyValue(1.5f, EditorPropertyField::POSITION_X) ==
          "1.50");
  REQUIRE(formatEditorPropertyValue(-90.0f, EditorPropertyField::ROTATION_Z) ==
          "-90.0");
}

TEST_CASE("a value of zero is never written as negative zero") {
  // -0.0 is what a drag back through zero leaves behind, and "-0.00" reads
  // as a bug rather than a number.
  REQUIRE(formatEditorPropertyValue(-0.0f, EditorPropertyField::POSITION_Y) ==
          "0.00");
}

TEST_CASE("each field's traits are the ones its own name promises") {
  // The labels and kinds are a table indexed by the enum's order, so a field
  // inserted without an entry beside it silently reads the next one's. The
  // table's length is asserted where it is declared; this is its order.
  REQUIRE(editorPropertyFieldLabel(EditorPropertyField::POSITION_X) ==
          "Position X");
  REQUIRE(editorPropertyFieldLabel(EditorPropertyField::RANGE) == "Range");
  REQUIRE(editorPropertyFieldKind(EditorPropertyField::POSITION_Z) ==
          EditorPropertyKind::DISTANCE);
  REQUIRE(editorPropertyFieldKind(EditorPropertyField::ROTATION_Z) ==
          EditorPropertyKind::ANGLE);
  REQUIRE(editorPropertyFieldKind(EditorPropertyField::DIRECTION_Y) ==
          EditorPropertyKind::AXIS);
  REQUIRE(editorPropertyFieldKind(EditorPropertyField::COLOR_B) ==
          EditorPropertyKind::UNIT);
  REQUIRE(editorPropertyFieldKind(EditorPropertyField::INTENSITY) ==
          EditorPropertyKind::FACTOR);
  REQUIRE(editorPropertyFieldKind(EditorPropertyField::RANGE) ==
          EditorPropertyKind::EXTENT);
}

TEST_CASE("a prop collides until told not to, and reads as 1 or 0") {
  EditorPlacement placement;
  REQUIRE(placement.collides);
  REQUIRE(editorPropertyValue(placement, EditorPropertyField::COLLIDES) ==
          1.0f);

  setEditorPropertyValue(placement, EditorPropertyField::COLLIDES, 0.0f);
  REQUIRE_FALSE(placement.collides);
  REQUIRE(editorPropertyValue(placement, EditorPropertyField::COLLIDES) ==
          0.0f);
}

TEST_CASE("a toggle is on at a half and above, and written as on or off") {
  REQUIRE(normalizeEditorPropertyValue(EditorPropertyField::COLLIDES, 0.4f) ==
          0.0f);
  REQUIRE(normalizeEditorPropertyValue(EditorPropertyField::COLLIDES, 0.5f) ==
          1.0f);
  REQUIRE(normalizeEditorPropertyValue(EditorPropertyField::COLLIDES, 7.0f) ==
          1.0f);
  REQUIRE(formatEditorPropertyValue(1.0f, EditorPropertyField::COLLIDES) ==
          "on");
  REQUIRE(formatEditorPropertyValue(0.0f, EditorPropertyField::COLLIDES) ==
          "off");
  REQUIRE(editorPropertyDragPerPixel(EditorPropertyField::COLLIDES) == 0.0f);
}
