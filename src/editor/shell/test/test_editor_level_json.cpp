#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-level-json.h>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-player-start-ops.h>
#include <nlohmann/json.hpp>
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

TEST_CASE("player starts round-trip as entities") {
  const std::vector<EditorAsset> assets = testAssets();
  EditorDocument written;
  written.player_starts = {{"start_01", 1, {2.5f, 3.5f, 0.0f}},
                           {"start_02", 4, {-1.5f, 6.5f, 1.0f}}};

  const std::string text = serializeEditorLevel(written, assets, "main");
  const std::optional<EditorLevelLoad> read = parseEditorLevel(text, assets);

  REQUIRE(read.has_value());
  REQUIRE(read->dropped_entities == 0);
  REQUIRE(read->document.player_starts.size() == 2);
  REQUIRE(read->document.player_starts[0].id == "start_01");
  REQUIRE(read->document.player_starts[0].player == 1);
  REQUIRE(read->document.player_starts[1].player == 4);
  REQUIRE(read->document.player_starts[1].position.x == -1.5f);
  REQUIRE(read->document.player_starts[1].position.z == 1.0f);
}

TEST_CASE("a player start is written in the format's entity shape") {
  EditorDocument document;
  EditorPlayerStart start = makeEditorPlayerStart(2, {1.5f, 1.5f, 0.0f});
  start.id = "start_01";
  document.player_starts.push_back(start);

  const nlohmann::json level =
      nlohmann::json::parse(serializeEditorLevel(document, {}, "main"));
  const nlohmann::json& entity = level.at("content").at("entities").at(0);

  REQUIRE(entity.at("id") == "start_01");
  REQUIRE(entity.at("definition") == "entity:player_start");
  REQUIRE(entity.at("properties").at("player") == 2);
  REQUIRE(entity.at("at").size() == 3);
}

TEST_CASE("an entity the editor has no definition for is dropped and counted") {
  const std::string text = R"({
    "schema": "simplish/level/1.0", "id": "main", "name": "main",
    "content": {"entities": [
      {"id": "spawn_north", "definition": "entity:spawn_point", "at": [0, 0, 0]},
      {"id": "start_01", "definition": "entity:player_start", "at": [1, 2, 0],
       "properties": {"player": 2}},
      "not an object"
    ]}})";

  const std::optional<EditorLevelLoad> read = parseEditorLevel(text, {});

  REQUIRE(read.has_value());
  REQUIRE(read->dropped_entities == 2);
  REQUIRE(read->document.player_starts.size() == 1);
  REQUIRE(read->document.player_starts[0].player == 2);
}

TEST_CASE("a hand-written player start gets an id and a player it lacks") {
  const std::string text = R"({
    "schema": "simplish/level/1.0", "id": "main", "name": "main",
    "content": {"entities": [
      {"definition": "entity:player_start", "at": [1, 2, 0]},
      {"definition": "entity:player_start", "properties": {"player": 9}}
    ]}})";

  const std::optional<EditorLevelLoad> read = parseEditorLevel(text, {});

  REQUIRE(read.has_value());
  REQUIRE(read->document.player_starts.size() == 2);
  REQUIRE(read->document.player_starts[0].id == "start_01");
  REQUIRE(read->document.player_starts[0].player == 1);
  REQUIRE(read->document.player_starts[1].id == "start_02");
  // Out of range reads as the nearest player a session has.
  REQUIRE(read->document.player_starts[1].player == 4);
}

TEST_CASE("whether a prop collides round-trips, and a missing flag is solid") {
  const std::vector<EditorAsset> assets = testAssets();
  EditorDocument written = testDocument();
  written.placements[1].collides = false;

  const auto read =
      parseEditorLevel(serializeEditorLevel(written, assets, "main"), assets);
  REQUIRE(read.has_value());
  REQUIRE(read->document.placements[0].collides);
  REQUIRE_FALSE(read->document.placements[1].collides);

  // A level saved before collision existed carries no flag at all.
  const std::string older = R"({
    "schema": "simplish/level/1.0", "id": "main", "name": "main",
    "content": {"props": [{"id": "cube_01", "asset": "shape:cube",
                           "at": [0, 0, 0]}]}})";
  const auto old_read = parseEditorLevel(older, assets);
  REQUIRE(old_read.has_value());
  REQUIRE(old_read->document.placements[0].collides);
}

TEST_CASE("a prop's clip round-trips, and none is written when it names none") {
  const std::vector<EditorAsset> assets = testAssets();
  EditorDocument written = testDocument();
  written.placements[1].animation = "walk";

  const std::string text = serializeEditorLevel(written, assets, "main");
  const auto read = parseEditorLevel(text, assets);
  REQUIRE(read.has_value());
  REQUIRE(read->document.placements[0].animation.empty());
  REQUIRE(read->document.placements[1].animation == "walk");
  // One prop names a clip, so the key appears exactly once in the file.
  REQUIRE(text.find("\"animation\"") == text.rfind("\"animation\""));
}

TEST_CASE("a start's character round-trips, and none is written for none") {
  const std::vector<EditorAsset> assets = testAssets();
  EditorDocument written = testDocument();
  written.player_starts.push_back(makeEditorPlayerStart(1, {1.5f, 1.5f, 0}));
  written.player_starts.push_back(makeEditorPlayerStart(2, {2.5f, 1.5f, 0}));
  written.player_starts[0].id = "start_01";
  written.player_starts[0].character = "character:scout";
  written.player_starts[1].id = "start_02";

  const std::string text = serializeEditorLevel(written, assets, "main");
  const auto read = parseEditorLevel(text, assets);

  REQUIRE(read.has_value());
  REQUIRE(read->document.player_starts[0].character == "character:scout");
  REQUIRE(read->document.player_starts[1].character.empty());
  REQUIRE(text.find("\"character\"") == text.rfind("\"character\""));
}

TEST_CASE("a hand-written character id reads as the character it names") {
  const std::string text = R"({
    "schema": "simplish/level/1.0", "id": "main", "name": "main",
    "content": {"entities": [
      {"id": "start_01", "definition": "entity:player_start",
       "at": [0.5, 0.5, 0], "properties": {"player": 1, "character": "scout"}},
      {"id": "start_02", "definition": "entity:player_start",
       "at": [1.5, 0.5, 0],
       "properties": {"player": 2, "character": "character:gone"}}]}})";

  const auto read = parseEditorLevel(text, testAssets());

  REQUIRE(read.has_value());
  REQUIRE(read->document.player_starts[0].character == "character:scout");
  // A character the project does not define is kept, not thrown away.
  REQUIRE(read->document.player_starts[1].character == "character:gone");
}
