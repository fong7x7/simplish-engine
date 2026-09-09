#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-property-ops.h>
#include <iterator>

using Catch::Approx;
using namespace eng;
using namespace eng::editor;

namespace {

/// Whether @p fields lists @p field.
bool lists(std::span<const EditorPropertyField> fields,
           EditorPropertyField field) {
  for (const EditorPropertyField listed : fields) {
    if (listed == field) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("a new light starts as the key light the scene already had") {
  const EditorLight light =
      makeEditorLight(EditorLightKind::DIRECTIONAL, {1.0f, 2.0f, 3.0f});

  REQUIRE(light.kind == EditorLightKind::DIRECTIONAL);
  REQUIRE(light.position.z == Approx(3.0f));
  REQUIRE(light.intensity == Approx(1.0f));
  REQUIRE(light.direction.x == Approx(MESH_KEY_LIGHT_DIRECTION.x));
  REQUIRE(light.color.x == Approx(1.0f));
}

TEST_CASE("a directional light lists a direction, a point light a position") {
  const auto directional = editorLightFields(EditorLightKind::DIRECTIONAL);
  const auto point = editorLightFields(EditorLightKind::POINT);

  REQUIRE(lists(directional, EditorPropertyField::DIRECTION_X));
  REQUIRE_FALSE(lists(directional, EditorPropertyField::POSITION_X));
  // A directional light never falls off, so a range would be a control that
  // changes nothing.
  REQUIRE_FALSE(lists(directional, EditorPropertyField::RANGE));

  REQUIRE(lists(point, EditorPropertyField::POSITION_X));
  REQUIRE(lists(point, EditorPropertyField::RANGE));
  REQUIRE_FALSE(lists(point, EditorPropertyField::DIRECTION_X));
}

TEST_CASE("both kinds list a colour and an intensity") {
  for (const EditorLightKind kind :
       {EditorLightKind::DIRECTIONAL, EditorLightKind::POINT}) {
    const auto fields = editorLightFields(kind);
    REQUIRE(lists(fields, EditorPropertyField::INTENSITY));
    REQUIRE(lists(fields, EditorPropertyField::COLOR_R));
    REQUIRE(lists(fields, EditorPropertyField::COLOR_G));
    REQUIRE(lists(fields, EditorPropertyField::COLOR_B));
  }
}

TEST_CASE("every property a light lists reads back what was written to it") {
  EditorLight light = makeEditorLight(EditorLightKind::POINT, {});
  for (const EditorPropertyField field :
       editorLightFields(EditorLightKind::POINT)) {
    setEditorLightValue(light, field, 0.5f);
    REQUIRE(editorLightValue(light, field) == Approx(0.5f));
  }
}

TEST_CASE("a direction reads back per component") {
  EditorLight light = makeEditorLight(EditorLightKind::DIRECTIONAL, {});
  setEditorLightValue(light, EditorPropertyField::DIRECTION_X, 0.25f);
  setEditorLightValue(light, EditorPropertyField::DIRECTION_Y, -0.5f);
  setEditorLightValue(light, EditorPropertyField::DIRECTION_Z, 1.0f);

  REQUIRE(light.direction.x == Approx(0.25f));
  REQUIRE(light.direction.y == Approx(-0.5f));
  REQUIRE(light.direction.z == Approx(1.0f));
}

TEST_CASE("a colour channel is held inside its own range") {
  EditorLight light = makeEditorLight(EditorLightKind::POINT, {});
  setEditorLightValue(light, EditorPropertyField::COLOR_R, 4.0f);
  setEditorLightValue(light, EditorPropertyField::COLOR_G, -1.0f);

  REQUIRE(light.color.x == Approx(1.0f));
  REQUIRE(light.color.y == Approx(0.0f));
}

TEST_CASE("a light cannot be dimmer than off or shorter than nothing") {
  EditorLight light = makeEditorLight(EditorLightKind::POINT, {});
  setEditorLightValue(light, EditorPropertyField::INTENSITY, -3.0f);
  setEditorLightValue(light, EditorPropertyField::RANGE, -1.0f);

  REQUIRE(light.intensity == Approx(0.0f));
  REQUIRE(light.range == Approx(0.0f));
}

TEST_CASE("a placement's fields mean nothing to a light") {
  EditorLight light = makeEditorLight(EditorLightKind::POINT, {});
  const EditorLight before = light;
  setEditorLightValue(light, EditorPropertyField::ROTATION_X, 90.0f);

  REQUIRE(editorLightValue(light, EditorPropertyField::ROTATION_X) ==
          Approx(0.0f));
  REQUIRE(light.direction.x == Approx(before.direction.x));
  REQUIRE(light.intensity == Approx(before.intensity));
}

TEST_CASE("a point light reaches the renderer with its range and position") {
  EditorLight light = makeEditorLight(EditorLightKind::POINT, {2.0f, 3.0f});
  light.range = 5.0f;
  const MeshLight mesh = makeMeshLight(light);

  REQUIRE(mesh.kind == MESH_LIGHT_POINT);
  REQUIRE(mesh.position.x == Approx(2.0f));
  REQUIRE(mesh.range == Approx(5.0f));
}

TEST_CASE("a directional light reaches the renderer with no range at all") {
  EditorLight light = makeEditorLight(EditorLightKind::DIRECTIONAL, {});
  // Its range is meaningless, and a renderer reading one would fall the
  // light off over a distance a directional light does not have.
  light.range = 5.0f;
  const MeshLight mesh = makeMeshLight(light);

  REQUIRE(mesh.kind == MESH_LIGHT_DIRECTIONAL);
  REQUIRE(mesh.range == Approx(0.0f));
  REQUIRE(mesh.direction.z == Approx(MESH_KEY_LIGHT_DIRECTION.z));
}
