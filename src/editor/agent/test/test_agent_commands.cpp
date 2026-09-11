#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <editor/agent/agent-state-json.h>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-property-ops.h>
#include <engine/input/input-action.h>
#include <game/content/behavior-lookup.h>
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

TEST_CASE("switching shading is queued once a project is open") {
  EditorShellState state;
  state.project.loaded = true;

  const AgentResult result =
      runAgentTool(state, "run_command", R"({"command": "set_shading_cel"})");

  REQUIRE(result.status == AgentStatus::OK);
  REQUIRE(result.host.command == EditorMenuCommand::SET_SHADING_CEL);
}

TEST_CASE("switching shading is refused with no project to record it in") {
  EditorShellState state;

  const AgentResult result = runAgentTool(
      state, "run_command", R"({"command": "set_shading_smooth"})");

  REQUIRE(result.status == AgentStatus::UNAVAILABLE);
  REQUIRE(result.host.kind != AgentHostRequestKind::RUN_COMMAND);
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

namespace {

/// A state with a project open and a playtest running, as the editor
/// leaves it once `start_playtest` has been carried out.
EditorShellState playingState() {
  EditorShellState state = stateWithAssets();
  state.playtest.mode = EditorPlayMode::PLAYING;
  return state;
}

}  // namespace

TEST_CASE("start_playtest needs a project, and queues the Play command") {
  EditorShellState none;
  REQUIRE(runAgentTool(none, "start_playtest", "{}").status ==
          AgentStatus::UNAVAILABLE);

  EditorShellState state = stateWithAssets();
  const AgentResult result = runAgentTool(state, "start_playtest", "{}");
  REQUIRE(result.status == AgentStatus::OK);
  // Straight to playing: an agent cannot click a card.
  REQUIRE(result.host.kind == AgentHostRequestKind::START_PLAYTEST);
  REQUIRE(result.host.character.empty());
}

TEST_CASE("start_playtest is refused while playing, stop_playtest while not") {
  EditorShellState playing = playingState();
  REQUIRE(runAgentTool(playing, "start_playtest", "{}").status ==
          AgentStatus::UNAVAILABLE);
  REQUIRE(runAgentTool(playing, "stop_playtest", "{}").host.command ==
          EditorMenuCommand::PLAYTEST);

  EditorShellState editing = stateWithAssets();
  REQUIRE(runAgentTool(editing, "stop_playtest", "{}").status ==
          AgentStatus::UNAVAILABLE);
}

TEST_CASE("send_input queues quantised input for player 1") {
  EditorShellState state = playingState();

  const json queued = call(
      state, "send_input",
      R"({"move_x": 1, "move_y": -0.5, "aim_x": 3, "fire": true, "ticks": 30})");

  REQUIRE(state.playtest.scripted.size() == 1);
  const EditorScriptedInput& input = state.playtest.scripted[0];
  REQUIRE(input.ticks == 30);
  REQUIRE(input.input.move_x == 32767);
  REQUIRE(input.input.move_y == -16384);
  // Out of range is full scale, as a stick pushed past its stop would be.
  REQUIRE(input.input.aim_x == 32767);
  REQUIRE(input.input.buttons == eng::input::INPUT_BUTTON_FIRE);
  REQUIRE(queued.at("queued_input_ticks") == 30);
}

TEST_CASE("send_input defaults to one idle tick and bounds the tick count") {
  EditorShellState state = playingState();
  (void)call(state, "send_input", "{}");
  REQUIRE(state.playtest.scripted[0].ticks == 1);
  REQUIRE(state.playtest.scripted[0].input == eng::sim::PlayerInput{});

  REQUIRE(runAgentTool(state, "send_input", R"({"ticks": 0})").status ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(runAgentTool(state, "send_input", R"({"ticks": 3601})").status ==
          AgentStatus::BAD_PARAMS);
}

TEST_CASE("send_input is refused when nothing is being played") {
  EditorShellState state = stateWithAssets();
  REQUIRE(runAgentTool(state, "send_input", R"({"move_x": 1})").status ==
          AgentStatus::UNAVAILABLE);
  REQUIRE(state.playtest.scripted.empty());
}

TEST_CASE("the level cannot be edited through the API while it is played") {
  EditorShellState state = playingState();

  for (const char* tool :
       {"place_asset", "add_light", "add_player_start", "set_property",
        "translate", "delete", "select", "undo", "redo"}) {
    INFO("tool " << tool);
    REQUIRE(
        runAgentTool(state, tool, R"({"asset": 0, "x": 0, "y": 0})").status ==
        AgentStatus::UNAVAILABLE);
  }
  REQUIRE(state.document.placements.empty());
  // Reading is still fine.
  REQUIRE(runAgentTool(state, "list_placements", "{}").status ==
          AgentStatus::OK);
}

TEST_CASE("a placement's collision is set like any property, and undone") {
  EditorShellState state = stateWithAssets();
  const json placed =
      call(state, "place_asset", R"({"asset": 0, "x": 0, "y": 0})");
  REQUIRE(placed.at("collides") == true);

  const json changed = call(
      state, "set_property",
      R"({"target": "placement", "index": 0, "field": "collides", "value": 0})");

  REQUIRE(changed.at("collides") == false);
  REQUIRE_FALSE(state.document.placements[0].collides);
  (void)call(state, "undo", "");
  REQUIRE(state.document.placements[0].collides);
}

TEST_CASE("a light and a player start refuse collides") {
  EditorShellState state = stateWithAssets();
  (void)call(state, "add_light", R"({"kind": "point", "x": 0, "y": 0})");
  (void)call(state, "add_player_start", R"({"x": 0, "y": 0})");

  REQUIRE(
      runAgentTool(
          state, "set_property",
          R"({"target": "light", "index": 0, "field": "collides", "value": 0})")
          .status == AgentStatus::BAD_PARAMS);
  REQUIRE(
      runAgentTool(
          state, "set_property",
          R"({"target": "player_start", "index": 0, "field": "collides", "value": 0})")
          .status == AgentStatus::BAD_PARAMS);
}

namespace {

/// A project whose third asset is a loaded rigged model with two clips.
EditorShellState stateWithRiggedAsset() {
  EditorShellState state = stateWithAssets();
  EditorAsset knight{.name = "knight",
                     .path = "/project/assets/knight.glb",
                     .relative_path = "knight.glb"};
  auto rig = std::make_shared<animation::Rig>();
  rig->clips = {animation::AnimationClip{"idle", 1.0f, {}},
                animation::AnimationClip{"walk", 1.0f, {}}};
  knight.rig = rig;
  state.assets.push_back(knight);
  return state;
}

}  // namespace

TEST_CASE("a rigged asset reports that it is, and names its clips") {
  EditorShellState state = stateWithRiggedAsset();
  const json asset = call(state, "get_asset", R"({"asset": "knight"})");
  REQUIRE(asset.at("rigged") == true);
  REQUIRE(asset.at("clips") == json::array({"idle", "walk"}));
  const json crate = call(state, "get_asset", R"({"asset": "crate"})");
  REQUIRE(crate.at("rigged") == false);
  REQUIRE(crate.at("clips").empty());
}

TEST_CASE("a placement's clip is set by name, reported, and undone") {
  EditorShellState state = stateWithRiggedAsset();
  const json placed =
      call(state, "place_asset", R"({"asset": "knight", "x": 0, "y": 0})");
  REQUIRE(placed.at("animation").get<std::string>().empty());

  const json changed =
      call(state, "set_animation",
           R"({"target": "placement", "index": 0, "clip": "walk"})");
  REQUIRE(changed.at("animation") == "walk");
  REQUIRE(state.document.placements[0].animation == "walk");
  (void)call(state, "undo", "");
  REQUIRE(state.document.placements[0].animation.empty());
}

TEST_CASE("a clip the model does not have is not found, and lists its clips") {
  EditorShellState state = stateWithRiggedAsset();
  (void)call(state, "place_asset", R"({"asset": "knight", "x": 0, "y": 0})");
  const AgentResult result =
      runAgentTool(state, "set_animation",
                   R"({"target": "placement", "index": 0, "clip": "dance"})");
  REQUIRE(result.status == AgentStatus::NOT_FOUND);
  REQUIRE(result.json.contains("idle, walk"));
}

TEST_CASE("a static model, a light, and an unloaded rig refuse a clip") {
  EditorShellState state = stateWithRiggedAsset();
  (void)call(state, "place_asset", R"({"asset": "crate", "x": 0, "y": 0})");
  (void)call(state, "add_light", R"({"kind": "point", "x": 0, "y": 0})");
  REQUIRE(runAgentTool(state, "set_animation",
                       R"({"target": "placement", "index": 0, "clip": "walk"})")
              .status == AgentStatus::BAD_PARAMS);
  REQUIRE(runAgentTool(state, "set_animation",
                       R"({"target": "light", "index": 0, "clip": "walk"})")
              .status == AgentStatus::BAD_PARAMS);
  state.assets[2].rig.reset();
  (void)call(state, "place_asset", R"({"asset": "knight", "x": 1, "y": 0})");
  REQUIRE(runAgentTool(state, "set_animation",
                       R"({"target": "placement", "index": 1, "clip": "walk"})")
              .status == AgentStatus::UNAVAILABLE);
}

namespace {

/// A project with two characters and one start for player 1.
EditorShellState stateWithCharacters() {
  EditorShellState state = stateWithAssets();
  state.characters.characters = {{"scout", "Scout", "", 7.0f, 3},
                                 {"tank", "Tank", "", 3.0f, 9}};
  (void)call(state, "add_player_start", R"({"x": 0.5, "y": 0.5})");
  return state;
}

}  // namespace

TEST_CASE("set_character names a start's character, and undo takes it off") {
  EditorShellState state = stateWithCharacters();

  const json named =
      call(state, "set_character",
           R"({"target": "player_start", "index": 0, "character": "tank"})");

  REQUIRE(named.at("character") == "character:tank");
  REQUIRE(state.document.player_starts[0].character == "character:tank");
  REQUIRE(state.history.actions.size() == 2);
  // The same character again, by name, changes nothing and records nothing.
  (void)call(state, "set_character",
             R"({"target": "selection", "character": "Tank"})");
  REQUIRE(state.history.actions.size() == 2);

  (void)call(state, "undo", "{}");
  REQUIRE(state.document.player_starts[0].character.empty());
}

TEST_CASE("set_character with no character names none") {
  EditorShellState state = stateWithCharacters();
  (void)call(state, "set_character",
             R"({"target": "player_start", "index": 0,
                 "character": "character:scout"})");

  const json bare =
      call(state, "set_character", R"({"target": "player_start", "index": 0})");

  REQUIRE(bare.at("character") == "");
  REQUIRE(state.document.player_starts[0].character.empty());
}

TEST_CASE(
    "set_character refuses an unknown character and anything but a start") {
  EditorShellState state = stateWithCharacters();
  (void)call(state, "place_asset", R"({"asset": 0, "x": 3, "y": 3})");

  const AgentResult unknown = runAgentTool(
      state, "set_character",
      R"({"target": "player_start", "index": 0, "character": "nope"})");
  REQUIRE(unknown.status == AgentStatus::NOT_FOUND);
  // The error names what there is to pick from.
  REQUIRE(unknown.json.find("scout, tank") != std::string::npos);
  REQUIRE(runAgentTool(state, "set_character",
                       R"({"target": "placement", "index": 0,
                           "character": "tank"})")
              .status == AgentStatus::BAD_PARAMS);
  REQUIRE(state.document.player_starts[0].character.empty());
}

TEST_CASE("start_playtest plays as the character named, or the default pick") {
  EditorShellState state = stateWithCharacters();

  REQUIRE(runAgentTool(state, "start_playtest", R"({"character": "Tank"})")
              .host.character == "tank");
  // No pick is the start's character, and with none the first one.
  REQUIRE(runAgentTool(state, "start_playtest", "{}").host.character ==
          "scout");
  state.document.player_starts[0].character = "character:tank";
  REQUIRE(runAgentTool(state, "start_playtest", "{}").host.character == "tank");
  REQUIRE(runAgentTool(state, "start_playtest", R"({"character": "nope"})")
              .status == AgentStatus::NOT_FOUND);
}

TEST_CASE("stop_playtest puts the character selector away") {
  EditorShellState state = stateWithCharacters();
  state.playtest.mode = EditorPlayMode::CHOOSING;

  const AgentResult result = runAgentTool(state, "stop_playtest", "{}");

  REQUIRE(result.status == AgentStatus::OK);
  REQUIRE(result.host.command == EditorMenuCommand::PLAYTEST);
  // And a playtest may be started from it, which is what picking does.
  REQUIRE(runAgentTool(state, "start_playtest", "{}").status ==
          AgentStatus::OK);
}

TEST_CASE("list_characters reports the table and what was wrong with it") {
  EditorShellState state = stateWithCharacters();
  state.characters.problems = {"a row was skipped"};

  const json listed = call(state, "list_characters", "{}");

  REQUIRE(listed.at("characters").size() == 2);
  REQUIRE(listed.at("characters")[1].at("ref") == "character:tank");
  REQUIRE(listed.at("characters")[1].at("health") == 9);
  REQUIRE(listed.at("characters")[0].at("move_speed") == Approx(7.0f));
  REQUIRE(listed.at("problems").size() == 1);
}

TEST_CASE("set_property scales a placement") {
  // The placement whitelist is read off the panel's own field list, so a
  // field the panel shows is one an agent can set without a second list to
  // forget it in.
  EditorShellState state = stateWithAssets();
  (void)call(state, "place_asset", R"({"asset": 0, "x": 0, "y": 0})");

  const json placed = call(
      state, "set_property",
      R"({"target": "placement", "index": 0, "field": "scale", "value": 2})");

  REQUIRE(placed.at("scale") == Approx(2.0f));
  REQUIRE(state.document.placements[0].scale == Approx(2.0f));
}

TEST_CASE("a scale is clamped as the panel would clamp it") {
  EditorShellState state = stateWithAssets();
  (void)call(state, "place_asset", R"({"asset": 0, "x": 0, "y": 0})");

  (void)call(
      state, "set_property",
      R"({"target": "placement", "index": 0, "field": "scale", "value": 0})");

  REQUIRE(state.document.placements[0].scale == Approx(EDITOR_SCALE_MIN));
}

TEST_CASE("a scale is one undoable edit") {
  EditorShellState state = stateWithAssets();
  (void)call(state, "place_asset", R"({"asset": 0, "x": 0, "y": 0})");
  (void)call(
      state, "set_property",
      R"({"target": "placement", "index": 0, "field": "scale", "value": 3})");

  (void)call(state, "undo", "{}");
  REQUIRE(state.document.placements[0].scale == Approx(1.0f));
}

TEST_CASE("scale is a placement's, not a light's") {
  EditorShellState state;
  (void)call(state, "add_light", R"({"kind": "point", "x": 0, "y": 0})");

  const AgentResult result =
      runAgentTool(state, "set_property",
                   R"({"target": "light", "index": 0, "field": "scale",
          "value": 2})");

  REQUIRE(result.status == AgentStatus::BAD_PARAMS);
}

namespace {

/// A project with one asset placed, and one behavior of its own: `zombie`.
EditorShellState stateWithAProp() {
  EditorShellState state = stateWithAssets();
  eng::game::BehaviorDefinition zombie =
      eng::game::resolveBehavior({}, "chase");
  zombie.id = "zombie";
  zombie.name = "Zombie";
  state.behaviors.behaviors.push_back(zombie);
  (void)call(state, "place_asset", R"({"asset": 0, "x": 3, "y": 3})");
  return state;
}

}  // namespace

TEST_CASE("list_behaviors lists the built-in behaviors, then the project's") {
  EditorShellState state = stateWithAProp();
  const json listed = call(state, "list_behaviors", "{}");
  const json& behaviors = listed.at("behaviors");

  REQUIRE(behaviors.size() == eng::game::builtInBehaviors().size() + 1);
  REQUIRE(behaviors[2].at("id") == "guard");
  REQUIRE(behaviors[2].at("built_in") == true);
  REQUIRE(behaviors[2].at("initial") == "watch");
  REQUIRE(behaviors.back().at("ref") == "behavior:zombie");
  REQUIRE(behaviors.back().at("built_in") == false);
  REQUIRE(listed.at("problems").empty());
}

TEST_CASE("set_behavior makes a prop an actor, and undo makes it scenery") {
  EditorShellState state = stateWithAProp();

  const json acting = call(state, "set_behavior",
                           R"({"target": "placement", "index": 0,
                               "behavior": "Zombie", "faction": "neutral"})");

  REQUIRE(acting.at("behavior") == "behavior:zombie");
  REQUIRE(acting.at("faction") == "neutral");
  REQUIRE(state.document.placements[0].faction == eng::game::Faction::NEUTRAL);
  REQUIRE(state.history.actions.size() == 2);
  // The faction alone, keeping the behavior.
  (void)call(state, "set_behavior",
             R"({"target": "selection", "faction": "friendly"})");
  REQUIRE(state.document.placements[0].behavior == "behavior:zombie");
  REQUIRE(state.history.actions.size() == 3);

  (void)call(state, "undo", "{}");
  (void)call(state, "undo", "{}");
  REQUIRE(state.document.placements[0].behavior.empty());
}

TEST_CASE("set_behavior with an empty behavior takes it away") {
  EditorShellState state = stateWithAProp();
  (void)call(state, "set_behavior",
             R"({"target": "placement", "index": 0, "behavior": "guard"})");
  (void)call(state, "set_behavior",
             R"({"target": "placement", "index": 0, "behavior": ""})");
  REQUIRE(state.document.placements[0].behavior.empty());
}

TEST_CASE("set_behavior refuses an unknown behavior, faction, or target") {
  EditorShellState state = stateWithAProp();
  (void)call(state, "add_light", R"({"kind": "point", "x": 0, "y": 0})");

  const AgentResult unknown = runAgentTool(
      state, "set_behavior",
      R"({"target": "placement", "index": 0, "behavior": "dancer"})");
  REQUIRE(unknown.status == AgentStatus::NOT_FOUND);
  REQUIRE(unknown.json.find("zombie") != std::string::npos);
  REQUIRE(runAgentTool(state, "set_behavior",
                       R"({"target": "placement", "index": 0,
                           "faction": "mauve"})")
              .status == AgentStatus::BAD_PARAMS);
  REQUIRE(
      runAgentTool(state, "set_behavior",
                   R"({"target": "light", "index": 0, "behavior": "guard"})")
          .status == AgentStatus::BAD_PARAMS);
  REQUIRE(state.document.placements[0].behavior.empty());
}

TEST_CASE("set_behavior is refused while the level is played") {
  EditorShellState state = stateWithAProp();
  state.playtest.mode = EditorPlayMode::PLAYING;
  REQUIRE(runAgentTool(
              state, "set_behavior",
              R"({"target": "placement", "index": 0, "behavior": "guard"})")
              .status != AgentStatus::OK);
  REQUIRE(state.document.placements[0].behavior.empty());
}
