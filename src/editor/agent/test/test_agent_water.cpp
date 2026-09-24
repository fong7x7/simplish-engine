#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <editor/shell/editor-terrains.h>
#include <editor/shell/editor-water-depths.h>
#include <engine/render-water/water-depth.h>
#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;
using namespace eng;
using namespace eng::editor;

namespace {

/// Run one tool and give back its parsed payload.
json call(EditorShellState& state, std::string_view tool,
          std::string_view params) {
  const AgentResult result = runAgentTool(state, tool, params);
  INFO("tool " << tool << " said " << result.json);
  REQUIRE(result.status == AgentStatus::OK);
  return json::parse(result.json);
}

}  // namespace

TEST_CASE("get_water reports the fidelity and what the water did") {
  EditorShellState state;
  state.graphics.file = "/tmp/graphics.json";
  state.water.drawn = true;
  state.water.samples_per_tile = 8;
  state.water.wet_samples = 128;
  state.water.energy = 0.25;
  state.water.pushes = 3;
  const json water = call(state, "get_water", "{}");
  CHECK(water["fidelity"] == "high");
  CHECK(water["fidelities"] == json::array({"flat", "low", "high"}));
  CHECK(water["file"] == "/tmp/graphics.json");
  CHECK(water["drawn"] == true);
  CHECK(water["samples_per_tile"] == 8);
  CHECK(water["water_tiles"] == 2.0);
  CHECK(water["energy"] == 0.25);
  CHECK(water["pushes"] == 3);
}

TEST_CASE("get_water counts no tiles when nothing is simulated") {
  EditorShellState state;
  state.graphics.water = WaterFidelity::FLAT;
  const json water = call(state, "get_water", "{}");
  CHECK(water["fidelity"] == "flat");
  CHECK(water["drawn"] == false);
  CHECK(water["water_tiles"] == 0.0);
}

TEST_CASE("set_water_fidelity sets the user's fidelity and asks for a save") {
  EditorShellState state;
  const uint64_t before = state.graphics.revision;
  const json water =
      call(state, "set_water_fidelity", R"({"fidelity": "low"})");
  CHECK(water["fidelity"] == "low");
  CHECK(state.graphics.water == WaterFidelity::LOW);
  CHECK(state.graphics.revision == before + 1);
}

TEST_CASE("set_water_fidelity refuses a word that names no fidelity") {
  EditorShellState state;
  const uint64_t before = state.graphics.revision;
  CHECK(runAgentTool(state, "set_water_fidelity", R"({"fidelity": "ultra"})")
            .status == AgentStatus::BAD_PARAMS);
  CHECK(runAgentTool(state, "set_water_fidelity", "{}").status ==
        AgentStatus::BAD_PARAMS);
  CHECK(state.graphics.water == WATER_DEFAULT_FIDELITY);
  CHECK(state.graphics.revision == before);
}

TEST_CASE("the View menu's water rows can be run by name") {
  EditorShellState state;
  for (const std::string_view name :
       {"set_water_flat", "set_water_low", "set_water_high"}) {
    const AgentResult result =
        runAgentTool(state, "run_command", json{{"command", name}}.dump());
    INFO(name << " said " << result.json);
    CHECK(result.status == AgentStatus::OK);
  }
}

namespace {

/// A shell state with sand from (0, 0) to (4, 3), and water over the four
/// columns from (0, 0).
EditorShellState withPond() {
  EditorShellState state;
  REQUIRE(runAgentTool(
              state, "paint_ground",
              R"({"terrain": "sand", "x": 0, "y": 0, "width": 5, "height": 4})")
              .status == AgentStatus::OK);
  REQUIRE(runAgentTool(state, "paint_water",
                       R"({"x": 0, "y": 0, "width": 4, "height": 4})")
              .status == AgentStatus::OK);
  return state;
}

}  // namespace

TEST_CASE("paint_water lays water over the ground and leaves the terrain") {
  EditorShellState state = withPond();
  CHECK(waterCellAt(state.document.water, {3, 3}).depth ==
        waterDepthUnits(WATER_DEFAULT_DEPTH));
  CHECK(state.document.water.depth.at({4, 0}) == 0);
  CHECK(state.document.ground.at({1, 1}) == *editorTerrainNamed("sand"));
  const json ground = call(state, "get_ground", "{}");
  CHECK(ground["water"][0] == "~~~~.");
  const json out = call(
      state, "paint_water",
      R"({"x": 4, "y": 0, "depth": "puddle", "color": "#ff0000", "opacity": 1})");
  CHECK(out["changed"] == 1);
  CHECK(waterCellAt(state.document.water, {4, 0}) ==
        WaterCell{1, 255, 0, 0, 255});
}

TEST_CASE("paint_water dries, and refuses what it cannot lay") {
  EditorShellState state = withPond();
  CHECK(call(state, "paint_water",
             R"({"x": 0, "y": 0, "width": 2, "height": 4, "dry": true})")
            ["changed"] == 8);
  CHECK(state.document.water.depth.at({1, 1}) == 0);
  for (const std::string_view params :
       {R"({"x": 0})", R"({"x": 0, "y": 0, "color": "red"})",
        R"({"x": 0, "y": 0, "opacity": 2})",
        R"({"x": 0, "y": 0, "depth": "ocean"})"}) {
    CHECK(runAgentTool(state, "paint_water", params).status ==
          AgentStatus::BAD_PARAMS);
  }
}

TEST_CASE("set_water_depth deepens the water in a rectangle and no more") {
  EditorShellState state = withPond();
  const json out =
      call(state, "set_water_depth",
           R"({"depth": "lake", "x": 2, "y": 0, "width": 3, "height": 4})");
  CHECK(out["changed"] == 8);
  CHECK(out["label"] == "Lake (3 tiles)");
  CHECK(waterDepthTiles(state.document.water.depth.at({3, 1})) == 3.0f);
  CHECK(state.document.water.depth.at({4, 1}) == 0);
  REQUIRE(runAgentTool(state, "undo", "{}").status == AgentStatus::OK);
  CHECK(waterDepthTiles(state.document.water.depth.at({3, 1})) ==
        WATER_DEFAULT_DEPTH);
}

TEST_CASE("a selected body of water is deepened, recoloured and dried") {
  EditorShellState state = withPond();
  const json selected =
      call(state, "select", R"({"target": "water", "x": 1.5, "y": 1.5})");
  CHECK(selected["target"] == "water");
  CHECK(selected["tiles"] == 16);
  call(state, "set_water_depth", R"({"depth": 0.25, "target": "selection"})");
  const json red =
      call(state, "set_property",
           R"({"target": "selection", "field": "color_r", "value": 1})");
  CHECK(red["fields"]["color_r"] == 1.0);
  CHECK(red["depth"] == "Shallows (0.25 tiles)");
  call(state, "delete", R"({"target": "selection"})");
  CHECK(state.document.water.depth.at({1, 1}) == 0);
  CHECK(state.document.ground.at({1, 1}) == *editorTerrainNamed("sand"));
}

TEST_CASE("a body of water has no field but its colour and opacity") {
  EditorShellState state = withPond();
  call(state, "select", R"({"target": "water", "x": 0, "y": 0})");
  CHECK(runAgentTool(state, "set_property",
                     R"({"target": "selection", "field": "scale", "value": 1})")
            .status == AgentStatus::BAD_PARAMS);
  CHECK(call(state, "set_property",
             R"({"target": "selection", "field": "opacity", "value": 0})")
            ["fields"]["opacity"] == 0.0);
}

TEST_CASE("set_water_depth refuses a depth it cannot keep") {
  EditorShellState state = withPond();
  for (const std::string_view params :
       {R"({"depth": "ocean", "x": 0, "y": 0})",
        R"({"depth": 40, "x": 0, "y": 0})", R"({"x": 0, "y": 0})",
        R"({"depth": "lake"})"}) {
    CHECK(runAgentTool(state, "set_water_depth", params).status ==
          AgentStatus::BAD_PARAMS);
  }
  CHECK(runAgentTool(state, "set_water_depth",
                     R"({"depth": "lake", "target": "selection"})")
            .status == AgentStatus::UNAVAILABLE);
  CHECK(runAgentTool(state, "select", R"({"target": "water", "x": 9, "y": 9})")
            .status == AgentStatus::NOT_FOUND);
}

TEST_CASE("get_water names every depth") {
  EditorShellState state;
  const json water = call(state, "get_water", "{}");
  REQUIRE(water["depths"].size() == EDITOR_WATER_DEPTH_COUNT);
  CHECK(water["depths"][0]["depth"] == "puddle");
  CHECK(water["depths"][3]["tiles"] == 3.0);
}
