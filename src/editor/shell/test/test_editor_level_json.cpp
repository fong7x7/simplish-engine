#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-level-json.h>
#include <editor/shell/editor-light-ops.h>
#include <optional>
#include <string>
#include <vector>

using namespace eng::editor;

namespace {

/// Two assets of the two sorts a project holds: a model on disk and a
/// built-in shape, both with the ids `assignEditorAssetIds` would give them.
std::vector<EditorAsset> testAssets() {
  std::vector<EditorAsset> assets(2);
  assets[0].name = "Crate";
  assets[0].relative_path = "props/crate.obj";
  assets[1].name = "Cube";
  assets[1].shape = EditorShapeKind::CUBE;
  assignEditorAssetIds(assets);
  return assets;
}

/// A document holding one prop of each asset and one point light.
EditorDocument testDocument() {
  EditorDocument document;
  document.placements.push_back(
      {"props_crate_01", 0, {2.5f, -3.0f, 0.5f}, {0.0f, 0.0f, 90.0f}});
  document.placements.push_back(
      {"cube_01", 1, {8.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}});
  EditorLight light =
      makeEditorLight(EditorLightKind::POINT, {4.0f, 4.0f, 3.0f});
  light.id = "point_01";
  light.color = {0.25f, 0.5f, 0.75f};
  light.intensity = 2.5f;
  light.range = 12.0f;
  document.lights.push_back(light);
  return document;
}

}  // namespace

TEST_CASE("a level round-trips everything the editor holds") {
  const std::vector<EditorAsset> assets = testAssets();
  const EditorDocument written = testDocument();

  const std::string text = serializeEditorLevel(written, assets, "My Project");
  const std::optional<EditorLevelLoad> read = parseEditorLevel(text, assets);

  REQUIRE(read.has_value());
  REQUIRE(read->dropped_props == 0);
  REQUIRE(read->document.placements.size() == 2);
  REQUIRE(read->document.lights.size() == 1);

  const EditorPlacement& crate = read->document.placements[0];
  REQUIRE(crate.id == "props_crate_01");
  REQUIRE(crate.asset == 0);
  REQUIRE(crate.position.x == 2.5f);
  REQUIRE(crate.position.y == -3.0f);
  REQUIRE(crate.position.z == 0.5f);
  REQUIRE(crate.rotation.z == 90.0f);
  REQUIRE(read->document.placements[1].asset == 1);

  const EditorLight& light = read->document.lights[0];
  REQUIRE(light.id == "point_01");
  REQUIRE(light.kind == EditorLightKind::POINT);
  REQUIRE(light.position.z == 3.0f);
  REQUIRE(light.color.y == 0.5f);
  REQUIRE(light.intensity == 2.5f);
  REQUIRE(light.range == 12.0f);
}

TEST_CASE("a prop names its asset by reference, not by index") {
  const std::vector<EditorAsset> assets = testAssets();
  const std::string text =
      serializeEditorLevel(testDocument(), assets, "My Project");

  REQUIRE(text.find("\"mesh:props_crate\"") != std::string::npos);
  REQUIRE(text.find("\"shape:cube\"") != std::string::npos);
  REQUIRE(text.find("\"simplish/level/1.0\"") != std::string::npos);
}

TEST_CASE("a level survives its assets being renumbered") {
  const std::vector<EditorAsset> assets = testAssets();
  const std::string text =
      serializeEditorLevel(testDocument(), assets, "My Project");

  // A file added ahead of the crate, which is exactly what a rescan does
  // to the numbering a placement was saved with.
  std::vector<EditorAsset> rescanned(1);
  rescanned[0].name = "Barrel";
  rescanned[0].relative_path = "props/barrel.obj";
  rescanned.push_back(assets[0]);
  rescanned.push_back(assets[1]);
  assignEditorAssetIds(rescanned);

  const std::optional<EditorLevelLoad> read = parseEditorLevel(text, rescanned);
  REQUIRE(read.has_value());
  REQUIRE(read->dropped_props == 0);
  REQUIRE(read->document.placements[0].asset == 1);
  REQUIRE(read->document.placements[1].asset == 2);
}

TEST_CASE("a prop whose asset is gone is dropped and counted") {
  const std::vector<EditorAsset> assets = testAssets();
  const std::string text =
      serializeEditorLevel(testDocument(), assets, "My Project");

  // Only the shape survives, so the crate has nothing to bind to.
  std::vector<EditorAsset> remaining{assets[1]};

  const std::optional<EditorLevelLoad> read = parseEditorLevel(text, remaining);
  REQUIRE(read.has_value());
  REQUIRE(read->dropped_props == 1);
  REQUIRE(read->document.placements.size() == 1);
  REQUIRE(read->document.placements[0].id == "cube_01");
  REQUIRE(read->document.lights.size() == 1);
}

TEST_CASE("a level from another schema version is refused") {
  const std::vector<EditorAsset> assets = testAssets();
  REQUIRE(!parseEditorLevel(R"({"schema":"simplish/level/2.0"})", assets));
  REQUIRE(!parseEditorLevel("{ not json", assets));
  REQUIRE(!parseEditorLevel("[]", assets));
}

TEST_CASE("a hand-written level fills in what it leaves out") {
  const std::vector<EditorAsset> assets = testAssets();
  const std::optional<EditorLevelLoad> read = parseEditorLevel(R"({
    "schema": "simplish/level/1.0",
    "id": "main",
    "name": "By Hand",
    "content": {
      "props": [{"asset": "shape:cube", "at": [1, 2, 3]}],
      "lights": [{"kind": "directional"}]
    }
  })",
                                                               assets);

  REQUIRE(read.has_value());
  REQUIRE(read->document.placements.size() == 1);
  // Minted, because nothing in the document may be unnameable.
  REQUIRE(read->document.placements[0].id == "cube_01");
  REQUIRE(read->document.placements[0].position.y == 2.0f);
  REQUIRE(read->document.placements[0].rotation.z == 0.0f);
  REQUIRE(read->document.lights.size() == 1);
  REQUIRE(read->document.lights[0].id == "directional_01");
  // Everything a dropped light of that kind would already have.
  REQUIRE(read->document.lights[0].intensity ==
          makeEditorLight(EditorLightKind::DIRECTIONAL, {}).intensity);
}

TEST_CASE("an empty level is a level") {
  const std::vector<EditorAsset> assets = testAssets();
  const std::string text = serializeEditorLevel({}, assets, "Empty");
  const std::optional<EditorLevelLoad> read = parseEditorLevel(text, assets);

  REQUIRE(read.has_value());
  REQUIRE(read->document.placements.empty());
  REQUIRE(read->document.lights.empty());
}
