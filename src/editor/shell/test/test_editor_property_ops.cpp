#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-property-ops.h>

using Catch::Approx;
using namespace eng::editor;

TEST_CASE("every property reads back what was written to it") {
  EditorPlacement placement;
  float written = 1.0f;
  for (EditorPropertyField field : EDITOR_PROPERTY_FIELDS) {
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
