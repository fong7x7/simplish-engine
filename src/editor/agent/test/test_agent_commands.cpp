#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <editor/agent/agent-state-json.h>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-light-ops.h>
#include <nlohmann/json.hpp>
#include <string>

using Catch::Approx;
using nlohmann::json;
using namespace eng;
using namespace eng::editor;

namespace {

/// A project with two assets in it, which is what every placement tool
/// needs before it can do anything.
EditorShellState stateWithAssets() {
  EditorShellState state;
  state.project.loaded = true;
  state.assets.push_back({.name = "crate", .relative_path = "props/crate.obj"});
  state.assets.push_back({.name = "barrel"});
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

TEST_CASE("placing an asset by name puts it where it was asked for") {
  EditorShellState state = stateWithAssets();

  const json placed =
      call(state, "place_asset", R"({"asset": "crate", "x": 3, "y": 4})");

  REQUIRE(state.document.placements.size() == 1);
  REQUIRE(placed.at("asset") == 0);
  REQUIRE(placed.at("position").at("x") == Approx(3.0f));
  REQUIRE(placed.at("position").at("z") == Approx(0.0f));
  // Dropped and selected, as a drag from the browser leaves it.
  REQUIRE(state.selection.kind == EditorSelectionKind::PLACEMENT);
  REQUIRE(state.history.actions.size() == 1);
}

TEST_CASE("an asset can also be named by index or by relative path") {
  EditorShellState state = stateWithAssets();

  (void)call(state, "place_asset", R"({"asset": 1, "x": 0, "y": 0})");
  (void)call(state, "place_asset",
             R"({"asset": "props/crate.obj", "x": 0, "y": 0})");

  REQUIRE(state.document.placements[0].asset == 1);
  REQUIRE(state.document.placements[1].asset == 0);
}

TEST_CASE("placing an asset nothing has scanned is not found") {
  EditorShellState state = stateWithAssets();

  const AgentResult result = runAgentTool(
      state, "place_asset", R"({"asset": "nope", "x": 0, "y": 0})");

  REQUIRE(result.status == AgentStatus::NOT_FOUND);
  REQUIRE(state.document.placements.empty());
}

TEST_CASE("a light drops at the height a browser drag would give it") {
  EditorShellState state;

  const json light =
      call(state, "add_light", R"({"kind": "point", "x": 2, "y": 2})");

  REQUIRE(state.document.lights.size() == 1);
  REQUIRE(light.at("kind") == "point");
  REQUIRE(light.at("position").at("z") == Approx(EDITOR_LIGHT_DROP_HEIGHT));
}

TEST_CASE("a light of no known kind is a bad call, not a default light") {
  EditorShellState state;

  const AgentResult result =
      runAgentTool(state, "add_light", R"({"kind": "spot", "x": 0, "y": 0})");

  REQUIRE(result.status == AgentStatus::BAD_PARAMS);
  REQUIRE(state.document.lights.empty());
}

TEST_CASE("shifting the only light to the right moves it along world X") {
  // The prompt this API was built to answer, end to end: find the one
  // light, then move it ten tiles right.
  EditorShellState state;
  (void)call(state, "add_light", R"({"kind": "point", "x": 5, "y": 5})");

  const json lights = call(state, "list_lights", "{}");
  REQUIRE(lights.at("lights").size() == 1);

  const json moved =
      call(state, "translate", R"({"target": "light", "index": 0, "dx": 10})");

  REQUIRE(moved.at("position").at("x") == Approx(15.0f));
  REQUIRE(moved.at("position").at("y") == Approx(5.0f));
  REQUIRE(state.document.lights[0].position.x == Approx(15.0f));
}

TEST_CASE("a move is one undoable edit, and undo puts it back") {
  EditorShellState state;
  (void)call(state, "add_light", R"({"kind": "point", "x": 5, "y": 5})");
  (void)call(state, "translate",
             R"({"target": "light", "index": 0, "dx": 10})");

  REQUIRE(state.history.actions.size() == 2);
  (void)call(state, "undo", "{}");

  REQUIRE(state.document.lights[0].position.x == Approx(5.0f));
  (void)call(state, "redo", "{}");
  REQUIRE(state.document.lights[0].position.x == Approx(15.0f));
}

TEST_CASE("a move of nothing records nothing") {
  EditorShellState state;
  (void)call(state, "add_light", R"({"kind": "point", "x": 5, "y": 5})");

  const AgentResult result =
      runAgentTool(state, "translate", R"({"target": "light", "index": 0})");

  REQUIRE(result.status == AgentStatus::OK);
  REQUIRE_FALSE(result.changed);
  REQUIRE(state.history.actions.size() == 1);
}

TEST_CASE("a tool can work on whatever the panel has selected") {
  EditorShellState state;
  (void)call(state, "add_light", R"({"kind": "point", "x": 0, "y": 0})");

  const json moved =
      call(state, "translate", R"({"target": "selection", "dz": 2})");

  REQUIRE(moved.at("index") == 0);
  REQUIRE(state.document.lights[0].position.z ==
          Approx(EDITOR_LIGHT_DROP_HEIGHT + 2.0f));
}

TEST_CASE("a target with nothing selected is unavailable, not a crash") {
  EditorShellState state;

  const AgentResult result =
      runAgentTool(state, "translate", R"({"target": "selection", "dx": 1})");

  REQUIRE(result.status == AgentStatus::UNAVAILABLE);
}

TEST_CASE("an index past the end of a list is not found") {
  EditorShellState state;

  const AgentResult result = runAgentTool(
      state, "translate", R"({"target": "light", "index": 7, "dx": 1})");

  REQUIRE(result.status == AgentStatus::NOT_FOUND);
}

TEST_CASE("set_property writes the value the editor would have stored") {
  EditorShellState state;
  (void)call(state, "add_light", R"({"kind": "point", "x": 0, "y": 0})");

  // Colour channels are fractions, so the panel would clamp this to one.
  const json light = call(
      state, "set_property",
      R"({"target": "light", "index": 0, "field": "color_r", "value": 4})");

  REQUIRE(light.at("color").at("x") == Approx(1.0f));
  REQUIRE(state.document.lights[0].color.x == Approx(1.0f));
}

TEST_CASE("a property one kind does not hold is refused with a reason") {
  EditorShellState state = stateWithAssets();
  (void)call(state, "place_asset", R"({"asset": 0, "x": 0, "y": 0})");

  const AgentResult result =
      runAgentTool(state, "set_property",
                   R"({"target": "placement", "index": 0, "field": "intensity",
          "value": 2})");

  REQUIRE(result.status == AgentStatus::BAD_PARAMS);
}

TEST_CASE("rotation is a placement's, not a light's") {
  EditorShellState state;
  (void)call(state, "add_light", R"({"kind": "point", "x": 0, "y": 0})");

  const AgentResult result =
      runAgentTool(state, "set_property",
                   R"({"target": "light", "index": 0, "field": "rotation_z",
          "value": 90})");

  REQUIRE(result.status == AgentStatus::BAD_PARAMS);
}

TEST_CASE("selecting and clearing move the properties panel's subject") {
  EditorShellState state;
  (void)call(state, "add_light", R"({"kind": "point", "x": 0, "y": 0})");

  (void)call(state, "select", R"({"target": "none"})");
  REQUIRE(state.selection.kind == EditorSelectionKind::NONE);

  const json selected = call(state, "select", R"({"target": "light",
                                                  "index": 0})");
  REQUIRE(selected.at("target") == "light");
  REQUIRE(state.selection.kind == EditorSelectionKind::LIGHT);
}

TEST_CASE("undo with nothing to undo says so rather than doing nothing") {
  EditorShellState state;

  const AgentResult result = runAgentTool(state, "undo", "{}");

  REQUIRE(result.status == AgentStatus::UNAVAILABLE);
}

TEST_CASE("a menu command the editor lists as disabled is refused") {
  EditorShellState state;

  const AgentResult result =
      runAgentTool(state, "run_command", R"({"command": "save"})");

  REQUIRE(result.status == AgentStatus::UNAVAILABLE);
}

TEST_CASE("saving is queued once there is a project to save into") {
  EditorShellState state;
  state.project.loaded = true;

  const AgentResult result =
      runAgentTool(state, "run_command", R"({"command": "save"})");

  REQUIRE(result.status == AgentStatus::OK);
  REQUIRE(result.host.kind == AgentHostRequestKind::RUN_COMMAND);
  REQUIRE(result.host.command == EditorMenuCommand::SAVE);
}

TEST_CASE("an enabled menu command is queued for the editor to run") {
  EditorShellState state;

  const AgentResult result =
      runAgentTool(state, "run_command", R"({"command": "toggle_grid"})");

  REQUIRE(result.status == AgentStatus::OK);
  REQUIRE(result.host.kind == AgentHostRequestKind::RUN_COMMAND);
  REQUIRE(result.host.command == EditorMenuCommand::TOGGLE_GRID);
}

TEST_CASE("switching projection is queued once a project is open") {
  EditorShellState state;
  state.project.loaded = true;

  const AgentResult result = runAgentTool(
      state, "run_command", R"({"command": "set_view_isometric"})");

  REQUIRE(result.status == AgentStatus::OK);
  REQUIRE(result.host.command == EditorMenuCommand::SET_VIEW_ISOMETRIC);
}

TEST_CASE("opening a project is queued with the path it was given") {
  EditorShellState state;

  const AgentResult result =
      runAgentTool(state, "open_project", R"({"path": "/tmp/level"})");

  REQUIRE(result.host.kind == AgentHostRequestKind::OPEN_PROJECT);
  REQUIRE(result.host.path == "/tmp/level");
}

TEST_CASE("a rescan needs a project to rescan") {
  EditorShellState state;

  REQUIRE(runAgentTool(state, "rescan_assets", "{}").status ==
          AgentStatus::UNAVAILABLE);

  state.project.loaded = true;
  REQUIRE(runAgentTool(state, "rescan_assets", "{}").host.kind ==
          AgentHostRequestKind::RESCAN_ASSETS);
}

TEST_CASE("the active tool can be chosen and read back") {
  EditorShellState state;

  (void)call(state, "set_tool", R"({"tool": "prop"})");

  REQUIRE(state.active_tool == EditorTool::PROP);
  REQUIRE(json::parse(agentStateJson(state)).at("active_tool") == "prop");
}

TEST_CASE("delete takes a placement out and undo puts it back") {
  EditorShellState state = stateWithAssets();
  (void)call(state, "place_asset", R"({"asset": 0, "x": 1, "y": 1})");
  (void)call(state, "place_asset", R"({"asset": 1, "x": 2, "y": 2})");

  const json removed =
      call(state, "delete", R"({"target": "placement", "index": 0})");

  REQUIRE(removed.at("removed") == true);
  REQUIRE(removed.at("index") == 0);
  REQUIRE(removed.at("asset") == 0);
  REQUIRE(state.document.placements.size() == 1);
  REQUIRE(state.document.placements[0].asset == 1);

  (void)call(state, "undo", "{}");
  REQUIRE(state.document.placements.size() == 2);
  REQUIRE(state.document.placements[0].asset == 0);
}

TEST_CASE("delete clears a selection that was on what it removed") {
  EditorShellState state = stateWithAssets();
  (void)call(state, "place_asset", R"({"asset": 0, "x": 0, "y": 0})");
  REQUIRE(state.selection.kind == EditorSelectionKind::PLACEMENT);

  (void)call(state, "delete", R"({"target": "selection"})");

  REQUIRE(state.selection.kind == EditorSelectionKind::NONE);
  REQUIRE(state.document.placements.empty());
}

TEST_CASE("delete moves a selection that sat after what it removed") {
  EditorShellState state = stateWithAssets();
  (void)call(state, "place_asset", R"({"asset": 0, "x": 0, "y": 0})");
  (void)call(state, "place_asset", R"({"asset": 1, "x": 1, "y": 1})");
  // Selected the second, then remove the first out from under it.
  (void)call(state, "select", R"({"target": "placement", "index": 1})");

  (void)call(state, "delete", R"({"target": "placement", "index": 0})");

  REQUIRE(state.selection.kind == EditorSelectionKind::PLACEMENT);
  REQUIRE(state.selection.index == 0);
}

TEST_CASE("delete removes a light and reports what it was") {
  EditorShellState state;
  (void)call(state, "add_light", R"({"kind": "point", "x": 2, "y": 2})");

  const json removed =
      call(state, "delete", R"({"target": "light", "index": 0})");

  REQUIRE(removed.at("kind") == "point");
  REQUIRE(removed.at("removed") == true);
  REQUIRE(state.document.lights.empty());
  REQUIRE(state.history.actions.size() == 2);
}

TEST_CASE("delete refuses an index the list does not have") {
  EditorShellState state = stateWithAssets();
  (void)call(state, "place_asset", R"({"asset": 0, "x": 0, "y": 0})");

  REQUIRE(runAgentTool(state, "delete", R"({"target": "placement",
                                           "index": 3})")
              .status == AgentStatus::NOT_FOUND);
  // Nothing selected is a state to change first, not a bad parameter.
  (void)call(state, "select", R"({"target": "none"})");
  REQUIRE(runAgentTool(state, "delete", R"({"target": "selection"})").status ==
          AgentStatus::UNAVAILABLE);
  REQUIRE(state.document.placements.size() == 1);
}

TEST_CASE("a removal is reported in the history under its own name") {
  EditorShellState state;
  (void)call(state, "add_light", R"({"kind": "directional", "x": 0, "y": 0})");
  (void)call(state, "delete", R"({"target": "light", "index": 0})");

  const json history = json::parse(agentHistoryJson(state));

  REQUIRE(history.at("actions").size() == 2);
  REQUIRE(history.at("actions")[1].at("kind") == "remove_light");
}

TEST_CASE("creating a level queues the work and names the level") {
  EditorShellState state;
  state.project.loaded = true;

  const AgentResult result =
      runAgentTool(state, "create_level", R"({"id": "roof"})");

  REQUIRE(result.status == AgentStatus::OK);
  REQUIRE(result.host.kind == AgentHostRequestKind::CREATE_LEVEL);
  REQUIRE(result.host.level == "roof");
  // The default is the careful one: an agent that wants the open level's
  // unwritten edits thrown away has to say so.
  REQUIRE(result.host.unsaved == EditorLevelUnsaved::REFUSE);
}

TEST_CASE("a level id the format would not take is a bad parameter") {
  EditorShellState state;
  state.project.loaded = true;

  const AgentResult result =
      runAgentTool(state, "create_level", R"({"id": "Roof Top"})");

  REQUIRE(result.status == AgentStatus::BAD_PARAMS);
  REQUIRE(result.host.kind == AgentHostRequestKind::NONE);
}

TEST_CASE("opening a level the project has not got is not found") {
  EditorShellState state;
  state.project.loaded = true;

  REQUIRE(runAgentTool(state, "open_level", R"({"id": "roof"})").status ==
          AgentStatus::NOT_FOUND);
}

TEST_CASE("a level tool with no project open is unavailable") {
  EditorShellState state;

  REQUIRE(runAgentTool(state, "create_level", R"({"id": "roof"})").status ==
          AgentStatus::UNAVAILABLE);
  REQUIRE(runAgentTool(state, "open_level", R"({"id": "main"})").status ==
          AgentStatus::UNAVAILABLE);
}

TEST_CASE("the unsaved policy is a word, and only the two it may be") {
  EditorShellState state;
  state.project.loaded = true;

  const AgentResult discard = runAgentTool(
      state, "create_level", R"({"id": "roof", "unsaved": "discard"})");
  REQUIRE(discard.status == AgentStatus::OK);
  REQUIRE(discard.host.unsaved == EditorLevelUnsaved::DISCARD);

  REQUIRE(runAgentTool(state, "create_level",
                       R"({"id": "roof", "unsaved": "maybe"})")
              .status == AgentStatus::BAD_PARAMS);
  REQUIRE(runAgentTool(state, "create_level", R"({"id": "roof", "unsaved": 1})")
              .status == AgentStatus::BAD_PARAMS);
}

TEST_CASE("unwritten edits stop a level switch before it is queued") {
  EditorShellState state;
  state.project.loaded = true;
  state.levels.push_back({"roof", true});
  performEditorAction(state.history, state.document,
                      {.kind = EditorActionKind::PLACE_ASSET, .index = 0});

  const AgentResult refused =
      runAgentTool(state, "create_level", R"({"id": "roof"})");

  REQUIRE(refused.status == AgentStatus::UNAVAILABLE);
  REQUIRE(refused.host.kind == AgentHostRequestKind::NONE);
}

TEST_CASE("adding a player start hands out players in order") {
  EditorShellState state = stateWithAssets();

  const json first = call(state, "add_player_start", R"({"x": 1.5, "y": 2.5})");
  const json second =
      call(state, "add_player_start", R"({"x": 3.5, "y": 2.5})");

  REQUIRE(state.document.player_starts.size() == 2);
  REQUIRE(first.at("player") == 1);
  REQUIRE(second.at("player") == 2);
  REQUIRE(first.at("ref") == "player_start:start_01");
  REQUIRE(first.at("position").at("x") == Approx(1.5f));
  REQUIRE(first.at("position").at("z") == Approx(0.0f));
  // Selected, as a drag from general > tools leaves it, and undoable.
  REQUIRE(state.selection.kind == EditorSelectionKind::PLAYER_START);
  REQUIRE(state.selection.index == 1);
  REQUIRE(state.history.actions.size() == 2);
}

TEST_CASE("a player start can be added for a named player, clamped to four") {
  EditorShellState state = stateWithAssets();

  const json named =
      call(state, "add_player_start", R"({"x": 0, "y": 0, "player": 3})");
  const json clamped =
      call(state, "add_player_start", R"({"x": 0, "y": 0, "player": 11})");

  REQUIRE(named.at("player") == 3);
  REQUIRE(clamped.at("player") == 4);
}

TEST_CASE("adding a player start without a position, or a player, is refused") {
  EditorShellState state = stateWithAssets();

  REQUIRE(runAgentTool(state, "add_player_start", R"({"x": 1})").status ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(runAgentTool(state, "add_player_start",
                       R"({"x": 1, "y": 1, "player": "two"})")
              .status == AgentStatus::BAD_PARAMS);
  REQUIRE(state.document.player_starts.empty());
}

TEST_CASE("a player start's player and position are set like any property") {
  EditorShellState state = stateWithAssets();
  (void)call(state, "add_player_start", R"({"x": 0, "y": 0})");

  const json changed = call(
      state, "set_property",
      R"({"target": "player_start", "index": 0, "field": "player", "value": 2})");
  (void)call(state, "set_property",
             R"({"target": "selection", "field": "position_y", "value": 7})");

  REQUIRE(changed.at("player") == 2);
  REQUIRE(state.document.player_starts[0].player == 2);
  REQUIRE(state.document.player_starts[0].position.y == Approx(7.0f));
  REQUIRE(state.history.actions.back().kind ==
          EditorActionKind::TRANSFORM_PLAYER_START);
}

TEST_CASE("a player start refuses a property it does not have") {
  EditorShellState state = stateWithAssets();
  (void)call(state, "add_player_start", R"({"x": 0, "y": 0})");

  const AgentResult result = runAgentTool(
      state, "set_property",
      R"({"target": "player_start", "index": 0, "field": "rotation_z", "value": 90})");

  REQUIRE(result.status == AgentStatus::BAD_PARAMS);
}

TEST_CASE("a placement and a light refuse a player") {
  EditorShellState state = stateWithAssets();
  (void)call(state, "place_asset", R"({"asset": 0, "x": 0, "y": 0})");
  (void)call(state, "add_light", R"({"kind": "point", "x": 0, "y": 0})");

  REQUIRE(
      runAgentTool(
          state, "set_property",
          R"({"target": "placement", "index": 0, "field": "player", "value": 2})")
          .status == AgentStatus::BAD_PARAMS);
  REQUIRE(
      runAgentTool(
          state, "set_property",
          R"({"target": "light", "index": 0, "field": "player", "value": 2})")
          .status == AgentStatus::BAD_PARAMS);
}

TEST_CASE("a player start moves, and is removed and put back by undo") {
  EditorShellState state = stateWithAssets();
  (void)call(state, "add_player_start", R"({"x": 1, "y": 1})");

  (void)call(state, "translate",
             R"({"target": "player_start", "index": 0, "dx": 2})");
  REQUIRE(state.document.player_starts[0].position.x == Approx(3.0f));

  const json removed =
      call(state, "delete", R"({"target": "player_start", "index": 0})");
  REQUIRE(removed.at("removed") == true);
  REQUIRE(removed.at("player") == 1);
  REQUIRE(state.document.player_starts.empty());

  (void)call(state, "undo", "");
  REQUIRE(state.document.player_starts.size() == 1);
  REQUIRE(state.selection.kind == EditorSelectionKind::PLAYER_START);
}

TEST_CASE("a player start is selected by name, and reported as selected") {
  EditorShellState state = stateWithAssets();
  (void)call(state, "add_player_start", R"({"x": 1, "y": 1})");
  (void)call(state, "select", R"({"target": "none"})");

  const json selected =
      call(state, "select", R"({"target": "player_start", "index": 0})");

  REQUIRE(selected.at("target") == "player_start");
  REQUIRE(selected.at("name") == "Player 1 Start");
  REQUIRE(selected.at("fields").at(0).at("name") == "player");
}
