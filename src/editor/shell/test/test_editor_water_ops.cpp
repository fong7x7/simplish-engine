#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-footstep-surfaces.h>
#include <editor/shell/editor-level-json.h>
#include <editor/shell/editor-terrains.h>
#include <editor/shell/editor-water-depth-choices.h>
#include <editor/shell/editor-water-depths.h>
#include <editor/shell/editor-water-ops.h>
#include <engine/render-water/water-depth.h>
#include <nlohmann/json.hpp>

using namespace eng;
using namespace eng::editor;

namespace {

/// A lake: water from (0, 0) to (3, 0), 3 tiles deep, red and opaque.
constexpr WaterCell LAKE{48, 200, 0, 0, 255};

/// A document with sand from (0, 0) to (5, 0) and the lake over its west
/// four cells.
EditorDocument lakeDocument() {
  EditorDocument document;
  const uint8_t sand = *editorTerrainNamed("sand");
  for (int32_t x = 0; x < 6; ++x) {
    document.ground.set({x, 0}, sand);
  }
  layEditorWater(document.water, {0, 0, 4, 1}, LAKE);
  return document;
}

/// Whether @p a and @p b hold the same water on every cell, whatever
/// rectangles their grids happen to have grown to.
bool sameWater(const WaterLayer& a, const WaterLayer& b) {
  return diffEditorWater(a, b).empty();
}

/// Every cell of the lake.
const GroundCell LAKE_CELLS[] = {{0, 0}, {1, 0}, {2, 0}, {3, 0}};

}  // namespace

TEST_CASE("water is a layer of its own, not a terrain") {
  CHECK_FALSE(editorTerrainNamed("water").has_value());
  CHECK(editorGroundCardBrush(EDITOR_WATER_CARD) == EditorGroundBrush::WATER);
  CHECK(editorGroundCardBrush(EDITOR_DRY_CARD) == EditorGroundBrush::DRY);
  CHECK(editorGroundCardBrush(0) == EditorGroundBrush::TERRAIN);
  CHECK(editorGroundCardName(EDITOR_TERRAIN_COUNT) == EDITOR_ERASER_NAME);
  CHECK(editorGroundCardTerrain(EDITOR_WATER_CARD) == 0);
}

TEST_CASE("laying water leaves the terrain under it, and keeps a body's look") {
  EditorDocument document = lakeDocument();
  CHECK(document.ground.at({1, 0}) == *editorTerrainNamed("sand"));
  // More water over the lake and past it: the lake keeps its colour, and
  // only the new cell takes the laid water's.
  const WaterCell pond{16, 10, 20, 30, 40};
  CHECK(layEditorWater(document.water, {3, 0, 2, 1}, pond));
  CHECK(waterCellAt(document.water, {3, 0}) ==
        WaterCell{16, LAKE.red, LAKE.green, LAKE.blue, LAKE.opacity});
  CHECK(waterCellAt(document.water, {4, 0}) == pond);
  CHECK(dryEditorWater(document.water, {4, 0, 1, 1}));
  CHECK(waterCellAt(document.water, {4, 0}).depth == 0);
}

TEST_CASE("a body's depth and look are set on its water only") {
  EditorDocument document = lakeDocument();
  const GroundCell with_sand[] = {{3, 0}, {5, 0}};
  CHECK(setEditorWaterDepth(document.water, with_sand, 8));
  CHECK(document.water.depth.at({3, 0}) == 8);
  CHECK(document.water.depth.at({5, 0}) == 0);
  CHECK(setEditorWaterValue(document.water, LAKE_CELLS,
                            EditorPropertyField::OPACITY, 0.2f));
  CHECK(editorWaterValue(waterCellAt(document.water, {0, 0}),
                         EditorPropertyField::OPACITY) ==
        static_cast<float>(51) / 255.0f);
  CHECK_FALSE(setEditorWaterValue(document.water, LAKE_CELLS,
                                  EditorPropertyField::SCALE, 0.5f));
}

TEST_CASE("a water edit is undone and redone whole") {
  EditorDocument document = lakeDocument();
  WaterLayer after = document.water;
  setEditorWaterValue(after, LAKE_CELLS, EditorPropertyField::COLOR_B, 1.0f);
  dryEditorWater(after, {0, 0, 1, 1});
  const std::optional<EditorAction> action = editorWaterEdit(document, after);
  REQUIRE(action.has_value());
  CHECK(action->ground.empty());
  EditorActionHistory history;
  performEditorAction(history, document, *action);
  CHECK(sameWater(document.water, after));
  REQUIRE(undoEditorAction(history, document));
  CHECK(sameWater(document.water, lakeDocument().water));
  CHECK_FALSE(editorWaterEdit(document, document.water).has_value());
}

TEST_CASE("a body of water is every cell of water joined, whatever its depth") {
  EditorDocument document = lakeDocument();
  setEditorWaterDepth(document.water, std::vector<GroundCell>{{2, 0}}, 2);
  layEditorWater(document.water, {9, 9, 1, 1}, LAKE);
  CHECK(connectedWaterCells(document.water, {0, 0}).size() == 4);
  CHECK(connectedWaterCells(document.water, {9, 9}).size() == 1);
  CHECK(connectedWaterCells(document.water, {5, 0}).empty());
}

TEST_CASE("the Depth row names the body's depth, or says it has none named") {
  EditorDocument document = lakeDocument();
  EditorWaterDepthChoices choices =
      editorWaterDepthChoices(document.water.depth, LAKE_CELLS);
  CHECK(choices.names.size() == EDITOR_WATER_DEPTH_COUNT);
  CHECK(EDITOR_WATER_DEPTHS[choices.current].word == "lake");
  setEditorWaterDepth(document.water, LAKE_CELLS, waterDepthUnits(1.75f));
  choices = editorWaterDepthChoices(document.water.depth, LAKE_CELLS);
  CHECK(choices.names.back() == "1.75 tiles");
  document.water.depth.set({0, 0}, 16);
  choices = editorWaterDepthChoices(document.water.depth, LAKE_CELLS);
  CHECK(choices.names.back() == "Mixed depths");
}

TEST_CASE("depths are named by word or number, and labelled") {
  CHECK(editorWaterDepthNamed("lake") == 3.0f);
  CHECK(editorWaterDepthNamed("0.5") == 0.5f);
  CHECK_FALSE(editorWaterDepthNamed("ocean").has_value());
  CHECK_FALSE(editorWaterDepthNamed("99").has_value());
  CHECK(EDITOR_WATER_DEPTHS[editorNearestWaterDepth(0.3f)].word == "shallows");
  CHECK(editorWaterDepthLabel(waterDepthUnits(3.0f)) == "Lake (3 tiles)");
  CHECK(editorWaterDepthLabel(waterDepthUnits(1.75f)) == "1.75 tiles");
}

TEST_CASE("a level keeps its water, and writes none it need not") {
  EditorDocument document;
  document.ground.set({0, 0}, 1);
  CHECK_FALSE(nlohmann::json::parse(serializeEditorLevel(document, {}, "r"))
                  .at("content")
                  .at("layers")
                  .contains("water"));
  document = lakeDocument();
  const std::optional<EditorLevelLoad> load =
      parseEditorLevel(serializeEditorLevel(document, {}, "r"), {});
  REQUIRE(load.has_value());
  CHECK(sameWater(load->document.water, document.water));
  CHECK(load->document.ground.at({1, 0}) == document.ground.at({1, 0}));
  CHECK(load->document.ground.at({5, 0}) == document.ground.at({5, 0}));
}

TEST_CASE("a level painted before water was a layer reads its water as one") {
  // Grass, then two cells of the old water terrain, one 3 tiles deep and
  // one at the old default.
  const std::string text = R"({
    "schema": "simplish/level/1.0", "id": "old", "name": "old",
    "content": {
      "bounds": {"min_x": 0, "min_y": 0, "width": 3, "height": 1},
      "tile_palette": ["tile:none", "tile:grass", "tile:dirt", "tile:sand",
                       "tile:water", "tile:stone"],
      "layers": {
        "terrain": {"encoding": "rle", "runs": [[1, 1], [4, 2]]},
        "water_depth": {"encoding": "rle", "step": 0.0625,
                        "runs": [[0, 1], [48, 1], [0, 1]]}}}})";
  const std::optional<EditorLevelLoad> load = parseEditorLevel(text, {});
  REQUIRE(load.has_value());
  const uint8_t sand = *editorTerrainNamed("sand");
  CHECK(load->document.ground.at({0, 0}) == *editorTerrainNamed("grass"));
  CHECK(load->document.ground.at({1, 0}) == sand);
  CHECK(waterCellAt(load->document.water, {1, 0}).depth == 48);
  CHECK(waterDepthTiles(load->document.water.depth.at({2, 0})) ==
        WATER_DEFAULT_DEPTH);
  CHECK(load->document.water.depth.at({0, 0}) == 0);
}

TEST_CASE("a step on water sounds like water, whatever is under it") {
  const EditorDocument document = lakeDocument();
  const game::FootstepSurfaces surfaces =
      makeEditorFootstepSurfaces(document, {});
  CHECK(surfaces.ground.at({1, 0}) ==
        static_cast<uint8_t>(game::FootstepSurface::WATER));
  CHECK(surfaces.ground.at({5, 0}) ==
        static_cast<uint8_t>(game::FootstepSurface::SAND));
}

// Req: docs/engine/water.md §6 — a body of water flows the way its Flow
// Direction row says, in degrees, at its Flow Speed, 0 to 1.
TEST_CASE("a body's flow is set in degrees and as a fraction of the fastest") {
  EditorDocument document = lakeDocument();
  CHECK(setEditorWaterValue(document.water, LAKE_CELLS,
                            EditorPropertyField::FLOW_DIRECTION, 90.0f));
  CHECK(setEditorWaterValue(document.water, LAKE_CELLS,
                            EditorPropertyField::FLOW_SPEED, 0.5f));
  const WaterCell river = waterCellAt(document.water, {2, 0});
  CHECK(river.flow_heading == 64);
  CHECK(river.flow_speed == 128);
  CHECK(editorWaterValue(river, EditorPropertyField::FLOW_DIRECTION) == 90.0f);
  CHECK(editorWaterByte(EditorPropertyField::FLOW_DIRECTION, -90.0f) == 192);
  CHECK(editorWaterByte(EditorPropertyField::FLOW_DIRECTION, 540.0f) == 128);
  CHECK(editorWaterValue(waterCellAt(document.water, {0, 0}),
                         EditorPropertyField::FLOW_DIRECTION) == 90.0f);
}

TEST_CASE("a level keeps its water's flow, and reads a file without any") {
  EditorDocument document = lakeDocument();
  setEditorWaterValue(document.water, LAKE_CELLS,
                      EditorPropertyField::FLOW_SPEED, 1.0f);
  const std::string text = serializeEditorLevel(document, {}, "r");
  const std::optional<EditorLevelLoad> load = parseEditorLevel(text, {});
  REQUIRE(load.has_value());
  CHECK(waterCellAt(load->document.water, {1, 0}).flow_speed == 255);
  nlohmann::json older = nlohmann::json::parse(text);
  older["content"]["layers"]["water"].erase("flow_heading");
  older["content"]["layers"]["water"].erase("flow_speed");
  const std::optional<EditorLevelLoad> still =
      parseEditorLevel(older.dump(), {});
  REQUIRE(still.has_value());
  CHECK(waterCellAt(still->document.water, {1, 0}).flow_speed == 0);
  CHECK(waterCellAt(still->document.water, {1, 0}).depth == LAKE.depth);
}
