#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-shape.h>

using namespace eng::editor;

namespace {

/// An asset as the scan would have produced it, at @p relative.
EditorAsset scannedAsset(const std::string& relative) {
  EditorAsset asset;
  asset.name = std::filesystem::path(relative).stem().string();
  asset.path = std::filesystem::path("/project/assets") / relative;
  asset.relative_path = relative;
  return asset;
}

/// A placement of the asset at @p index, already identified.
EditorPlacement identifiedPlacement(std::string id, size_t index) {
  EditorPlacement placement;
  placement.id = std::move(id);
  placement.asset = index;
  return placement;
}

}  // namespace

TEST_CASE("an identifier is lowercase snake_case") {
  REQUIRE(makeEditorIdentifier("Crate") == "crate");
  REQUIRE(makeEditorIdentifier("Gate North") == "gate_north");
  REQUIRE(makeEditorIdentifier("transit-station") == "transit_station");
}

TEST_CASE("runs of unusable characters collapse to one underscore") {
  REQUIRE(makeEditorIdentifier("a -- b") == "a_b");
  REQUIRE(makeEditorIdentifier("__lead and trail__") == "lead_and_trail");
}

TEST_CASE("an identifier never starts with a digit") {
  // Ids are emitted as the names of generated C++ symbols, and `2x2_slab`
  // is not one.
  REQUIRE(makeEditorIdentifier("2x2 slab") == "_2x2_slab");
}

TEST_CASE("text with nothing usable in it still yields an identifier") {
  REQUIRE(makeEditorIdentifier("---") == EDITOR_ID_FALLBACK);
  REQUIRE(makeEditorIdentifier("") == EDITOR_ID_FALLBACK);
}

TEST_CASE("an asset id comes from its path, extension dropped") {
  REQUIRE(editorAssetIdFromPath("crate.obj") == "crate");
  REQUIRE(editorAssetIdFromPath("props/crate.obj") == "props_crate");
  REQUIRE(editorAssetIdFromPath("terrain/rocks/boulder.obj") ==
          "terrain_rocks_boulder");
}

TEST_CASE("an asset id does not depend on what else the scan found") {
  // This is the whole point of deriving it from the path. A numeric suffix
  // handed out in scan order would renumber an existing asset the moment an
  // unrelated file was added before it, and every level naming the old id
  // would then be wrong.
  std::vector<EditorAsset> alone{scannedAsset("props/crate.obj")};
  std::vector<EditorAsset> crowded{scannedAsset("debris/crate.obj"),
                                   scannedAsset("props/crate.obj")};
  assignEditorAssetIds(alone);
  assignEditorAssetIds(crowded);

  REQUIRE(alone[0].id == "props_crate");
  REQUIRE(crowded[1].id == "props_crate");
  REQUIRE(crowded[0].id == "debris_crate");
}

TEST_CASE("two paths that slugify alike still get distinct ids") {
  std::vector<EditorAsset> assets{scannedAsset("my-crate.obj"),
                                  scannedAsset("my_crate.obj")};
  assignEditorAssetIds(assets);
  REQUIRE(assets[0].id != assets[1].id);
}

TEST_CASE("a built-in shape is named for itself, not for a path") {
  std::vector<EditorAsset> assets;
  appendEditorShapeAssets(assets);
  assignEditorAssetIds(assets);

  REQUIRE(assets[0].id == "cube");
  REQUIRE(editorAssetRef(assets[0]) == "shape:cube");
}

TEST_CASE("a model on disk is referenced as a mesh") {
  std::vector<EditorAsset> assets{scannedAsset("props/crate.obj")};
  assignEditorAssetIds(assets);
  REQUIRE(editorAssetRef(assets[0]) == "mesh:props_crate");
}

TEST_CASE("a placement is numbered from the asset it instances") {
  std::vector<EditorAsset> assets{scannedAsset("crate.obj")};
  assignEditorAssetIds(assets);
  EditorDocument document;

  REQUIRE(mintEditorPlacementId(document, assets[0]) == "crate_01");
  document.placements.push_back(identifiedPlacement("crate_01", 0));
  REQUIRE(mintEditorPlacementId(document, assets[0]) == "crate_02");
}

TEST_CASE("a freed number is handed out again") {
  // Undo removes a placement and frees its id; redo restores that same
  // placement carrying the id it already had, so nothing is ever handed the
  // same id twice while both exist.
  std::vector<EditorAsset> assets{scannedAsset("crate.obj")};
  assignEditorAssetIds(assets);
  EditorDocument document;
  document.placements.push_back(identifiedPlacement("crate_02", 0));

  REQUIRE(mintEditorPlacementId(document, assets[0]) == "crate_01");
}

TEST_CASE("a placement of a vanished asset is still nameable") {
  EditorDocument document;
  REQUIRE(mintEditorPlacementId(document, EditorAsset{}) == "prop_01");
}

TEST_CASE("a light is numbered from its own kind") {
  EditorDocument document;
  REQUIRE(mintEditorLightId(document, EditorLightKind::POINT) == "point_01");

  EditorLight light = makeEditorLight(EditorLightKind::POINT, {});
  light.id = "point_01";
  document.lights.push_back(light);
  REQUIRE(mintEditorLightId(document, EditorLightKind::POINT) == "point_02");
  // A different kind counts separately, so the two read as what they are.
  REQUIRE(mintEditorLightId(document, EditorLightKind::DIRECTIONAL) ==
          "directional_01");
}

TEST_CASE("a light is referenced as a light") {
  EditorLight light = makeEditorLight(EditorLightKind::POINT, {});
  light.id = "point_03";
  REQUIRE(editorLightRef(light) == "light:point_03");
}

TEST_CASE("a placement is referenced as a prop") {
  REQUIRE(editorPlacementRef(identifiedPlacement("crate_01", 0)) ==
          "prop:crate_01");
}

TEST_CASE("every kind has its own reference prefix") {
  // The prefix is what lets the content generator type-check a reference
  // without loading its target, so no two kinds may share one.
  REQUIRE(editorQualifiedId(EditorIdKind::MESH, "a") == "mesh:a");
  REQUIRE(editorQualifiedId(EditorIdKind::SHAPE, "a") == "shape:a");
  REQUIRE(editorQualifiedId(EditorIdKind::PROP, "a") == "prop:a");
  REQUIRE(editorQualifiedId(EditorIdKind::LIGHT, "a") == "light:a");
}

TEST_CASE("a rescan that adds a file keeps every placement") {
  // The case that used to cost a designer the whole level: drop a crate,
  // save a new model into the project, and the browser rescans.
  std::vector<EditorAsset> before{scannedAsset("props/crate.obj")};
  assignEditorAssetIds(before);
  EditorDocument document;
  document.placements.push_back(identifiedPlacement("props_crate_01", 0));

  // The new file sorts first, so the crate is asset 1 now rather than 0.
  std::vector<EditorAsset> after{scannedAsset("debris/barrel.obj"),
                                 scannedAsset("props/crate.obj")};
  assignEditorAssetIds(after);

  REQUIRE_FALSE(rebindPlacementAssets(document, {before[0].id}, after));
  REQUIRE(document.placements.size() == 1);
  REQUIRE(document.placements[0].asset == 1);
  // The placement's own id is untouched: it names this crate, not the
  // asset's position in anything.
  REQUIRE(document.placements[0].id == "props_crate_01");
}

TEST_CASE("a rescan that removes a file drops what it placed") {
  std::vector<EditorAsset> before{scannedAsset("props/crate.obj"),
                                  scannedAsset("props/lamp.obj")};
  assignEditorAssetIds(before);
  EditorDocument document;
  document.placements.push_back(identifiedPlacement("props_crate_01", 0));
  document.placements.push_back(identifiedPlacement("props_lamp_01", 1));

  std::vector<EditorAsset> after{scannedAsset("props/lamp.obj")};
  assignEditorAssetIds(after);

  REQUIRE(rebindPlacementAssets(document, {before[0].id, before[1].id}, after));
  REQUIRE(document.placements.size() == 1);
  REQUIRE(document.placements[0].id == "props_lamp_01");
  REQUIRE(document.placements[0].asset == 0);
}

TEST_CASE("a rescan leaves the lights alone") {
  // They name no asset, so nothing about them can go stale.
  EditorDocument document;
  EditorLight light = makeEditorLight(EditorLightKind::POINT, {});
  light.id = "point_01";
  document.lights.push_back(light);

  REQUIRE_FALSE(rebindPlacementAssets(document, {}, {}));
  REQUIRE(document.lights.size() == 1);
  REQUIRE(document.lights[0].id == "point_01");
}

TEST_CASE("a placement naming an index the old list never had is dropped") {
  EditorDocument document;
  document.placements.push_back(identifiedPlacement("stale_01", 7));
  REQUIRE(rebindPlacementAssets(document, {}, {}));
  REQUIRE(document.placements.empty());
}

TEST_CASE("a player start is numbered from start, not from its player") {
  EditorDocument document;
  EditorPlayerStart first = makeEditorPlayerStart(3, {});
  first.id = mintEditorPlayerStartId(document);
  document.player_starts.push_back(first);

  REQUIRE(first.id == "start_01");
  // Which player a start is for can change; its id cannot, so the id says
  // nothing about the player.
  REQUIRE(mintEditorPlayerStartId(document) == "start_02");
  REQUIRE(editorPlayerStartRef(first) == "player_start:start_01");
}
