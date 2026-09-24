#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-terrains.h>
#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;
using namespace eng::editor;

namespace {

/// A project with nothing painted yet.
EditorShellState openProject() {
  EditorShellState state;
  state.project.loaded = true;
  return state;
}

/// Run one tool and give back its parsed payload.
json call(EditorShellState& state, std::string_view tool,
          std::string_view params) {
  const AgentResult result = runAgentTool(state, tool, params);
  INFO("tool " << tool << " said " << result.json);
  REQUIRE(result.status == AgentStatus::OK);
  return json::parse(result.json);
}

}  // namespace

TEST_CASE("painting a rectangle lays that terrain on every cell of it") {
  EditorShellState state = openProject();

  const json painted = call(state, "paint_ground",
                            R"({"terrain": "road", "x": 2, "y": 3,
                                "width": 4, "height": 1})");

  REQUIRE(painted.at("changed") == 4);
  REQUIRE(painted.at("painted").at("width") == 4);
  REQUIRE(state.document.ground.at({5, 3}) == *editorTerrainNamed("road"));
  REQUIRE(state.history.actions.size() == 1);
}

TEST_CASE("one paint is one undo") {
  EditorShellState state = openProject();
  (void)call(state, "paint_ground",
             R"({"terrain": "sand", "x": 0, "y": 0, "width": 3, "height": 3})");

  (void)call(state, "undo", "{}");

  REQUIRE(state.document.ground.empty());
}

TEST_CASE("painting what is already there records nothing") {
  EditorShellState state = openProject();
  (void)call(state, "paint_ground", R"({"terrain": "grass", "x": 1, "y": 1})");
  const json again =
      call(state, "paint_ground", R"({"terrain": "grass", "x": 1, "y": 1})");

  REQUIRE(again.at("changed") == 0);
  REQUIRE(state.history.actions.size() == 1);
}

TEST_CASE("none erases back to bare ground") {
  EditorShellState state = openProject();
  (void)call(state, "paint_ground",
             R"({"terrain": "dirt", "x": 0, "y": 0, "width": 2})");
  (void)call(state, "paint_ground", R"({"terrain": "none", "x": 0, "y": 0})");

  REQUIRE(state.document.ground.at({0, 0}) == 0);
  REQUIRE(state.document.ground.at({1, 0}) == 2);
}

TEST_CASE("a terrain nobody has, or a rectangle too large, is refused") {
  EditorShellState state = openProject();
  REQUIRE(runAgentTool(state, "paint_ground",
                       R"({"terrain": "lava", "x": 0, "y": 0})")
              .status == AgentStatus::BAD_PARAMS);
  REQUIRE(runAgentTool(state, "paint_ground",
                       R"({"terrain": "sand", "x": 0, "y": 0, "width": 999})")
              .status == AgentStatus::BAD_PARAMS);
  REQUIRE(
      runAgentTool(state, "paint_ground", R"({"terrain": "sand"})").status ==
      AgentStatus::BAD_PARAMS);
  REQUIRE(state.document.ground.empty());
}

TEST_CASE("get_ground reports the painted cells a row at a time") {
  EditorShellState state = openProject();
  (void)call(state, "paint_ground",
             R"({"terrain": "grass", "x": 0, "y": 0, "width": 3})");
  (void)call(state, "paint_ground", R"({"terrain": "road", "x": 1, "y": 1})");

  const json ground = call(state, "get_ground", "{}");

  REQUIRE(ground.at("window").at("height") == 2);
  // Southmost row first, each west to east.
  REQUIRE(ground.at("rows") == json::parse(R"(["111", ".5."])"));
  REQUIRE(ground.at("terrains").at(0).at("terrain") == "none");
  REQUIRE(ground.at("terrains").at(5).at("terrain") == "road");
}

TEST_CASE("get_ground reads any window, and refuses one too large") {
  EditorShellState state = openProject();
  (void)call(state, "paint_ground", R"({"terrain": "stone", "x": 5, "y": 5})");

  const json window =
      call(state, "get_ground", R"({"x": 4, "y": 5, "width": 3, "height": 1})");
  REQUIRE(window.at("rows") == json::parse(R"([".4."])"));
  REQUIRE(runAgentTool(state, "get_ground",
                       R"({"x": 0, "y": 0, "width": 1000, "height": 1000})")
              .status == AgentStatus::BAD_PARAMS);
}

TEST_CASE("select picks the patch a tile is in, and get_selection says so") {
  EditorShellState state = openProject();
  (void)call(state, "paint_ground",
             R"({"terrain": "sand", "x": 0, "y": 0, "width": 4, "height": 2})");

  const json selected =
      call(state, "select", R"({"target": "ground", "x": 2.5, "y": 1.5})");

  REQUIRE(selected.at("target") == "ground");
  REQUIRE(selected.at("terrain") == "sand");
  REQUIRE(selected.at("tiles") == 8);
  REQUIRE(selected.at("bounds").at("width") == 4);
  REQUIRE(
      runAgentTool(state, "select", R"({"target": "ground", "x": 9, "y": 9})")
          .status == AgentStatus::NOT_FOUND);
}

TEST_CASE("the selected patch is repainted in one edit, and erased by delete") {
  EditorShellState state = openProject();
  (void)call(state, "paint_ground",
             R"({"terrain": "sand", "x": 0, "y": 0, "width": 3})");
  (void)call(state, "select", R"({"target": "ground", "x": 0, "y": 0})");

  const json repainted = call(state, "paint_ground",
                              R"({"target": "selection", "terrain": "road"})");
  REQUIRE(repainted.at("changed") == 3);
  REQUIRE(state.document.ground.at({2, 0}) == *editorTerrainNamed("road"));
  REQUIRE(state.history.actions.size() == 2);

  (void)call(state, "delete", R"({"target": "selection"})");
  REQUIRE(state.document.ground.empty());
  REQUIRE(state.selection.kind == EditorSelectionKind::NONE);
}

TEST_CASE("a patch of ground has no numbers to set, and does not move") {
  EditorShellState state = openProject();
  (void)call(state, "paint_ground", R"({"terrain": "dirt", "x": 0, "y": 0})");
  (void)call(state, "select", R"({"target": "ground", "x": 0, "y": 0})");
  REQUIRE(runAgentTool(state, "set_property",
                       R"({"target": "selection", "field": "x", "value": 3})")
              .status == AgentStatus::BAD_PARAMS);
  REQUIRE(
      runAgentTool(state, "translate", R"({"target": "selection", "dx": 1})")
          .status == AgentStatus::BAD_PARAMS);
  REQUIRE(runAgentTool(state, "paint_ground",
                       R"({"target": "selection", "terrain": "lava"})")
              .status == AgentStatus::BAD_PARAMS);
}
