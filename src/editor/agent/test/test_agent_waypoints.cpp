#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <editor/shell/editor-action-ops.h>
#include <nlohmann/json.hpp>
#include <string>

using Catch::Approx;
using nlohmann::json;
using namespace eng::editor;

namespace {

/// A project with one asset in it, for a prop to patrol with.
EditorShellState stateWithAsset() {
  EditorShellState state;
  state.project.loaded = true;
  state.assets.push_back({.name = "crate", .relative_path = "props/crate.obj"});
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

/// The status one tool call ends in.
AgentStatus statusOf(EditorShellState& state, std::string_view tool,
                     std::string_view params) {
  return runAgentTool(state, tool, params).status;
}

}  // namespace

TEST_CASE("add_waypoint lays a route out in the order it is called") {
  EditorShellState state = stateWithAsset();

  const json first = call(state, "add_waypoint", R"({"x": 1.5, "y": 1.5})");
  const json second = call(state, "add_waypoint", R"({"x": 4.5, "y": 1.5})");

  REQUIRE(first.at("route") == 1);
  REQUIRE(first.at("order") == 1);
  REQUIRE(first.at("id") == "waypoint_01");
  REQUIRE(first.at("ref") == "waypoint:waypoint_01");
  // The first is selected, so the second follows it on its route.
  REQUIRE(second.at("route") == 1);
  REQUIRE(second.at("order") == 2);
  REQUIRE(second.at("index") == 1);
  REQUIRE(state.selection.kind == EditorSelectionKind::WAYPOINT);
  REQUIRE(state.history.actions.size() == 2);
}

TEST_CASE("add_waypoint follows the selected waypoint's route") {
  EditorShellState state = stateWithAsset();
  (void)call(state, "add_waypoint", R"({"x": 1, "y": 1, "route": 4})");

  const json next = call(state, "add_waypoint", R"({"x": 2, "y": 1})");
  const json placed = call(state, "add_waypoint",
                           R"({"x": 3, "y": 1, "route": 12, "order": 7})");

  REQUIRE(next.at("route") == 4);
  REQUIRE(next.at("order") == 2);
  REQUIRE(placed.at("route") == 9);
  REQUIRE(placed.at("order") == 7);
}

TEST_CASE("add_waypoint without a position, or with a word for a route, is "
          "refused") {
  EditorShellState state = stateWithAsset();
  REQUIRE(statusOf(state, "add_waypoint", R"({"x": 1})") ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(statusOf(state, "add_waypoint",
                   R"({"x": 1, "y": 1, "route": "north"})") ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(state.document.waypoints.empty());
}

TEST_CASE("a waypoint's route is set, and it is moved, by the generic tools") {
  EditorShellState state = stateWithAsset();
  (void)call(state, "add_waypoint", R"({"x": 1, "y": 1})");

  const json routed = call(
      state, "set_property",
      R"({"target": "waypoint", "index": 0, "field": "route", "value": 3})");
  const json moved =
      call(state, "translate", R"({"target": "selection", "dx": 2})");

  REQUIRE(routed.at("route") == 3);
  REQUIRE(moved.at("position").at("x") == Approx(3.0f));
  REQUIRE(statusOf(state, "set_property",
                   R"({"target": "waypoint", "index": 0, "field": "scale",
                       "value": 2})") == AgentStatus::BAD_PARAMS);
}

TEST_CASE("a waypoint is deleted by the generic tool, and undo puts it back") {
  EditorShellState state = stateWithAsset();
  (void)call(state, "add_waypoint", R"({"x": 1, "y": 1})");

  const json removed =
      call(state, "delete", R"({"target": "waypoint", "index": 0})");

  REQUIRE(removed.at("removed") == true);
  REQUIRE(state.document.waypoints.empty());
  (void)call(state, "undo", "{}");
  REQUIRE(state.document.waypoints.size() == 1);
}

TEST_CASE("a selected waypoint reports its route and place") {
  EditorShellState state = stateWithAsset();
  (void)call(state, "add_waypoint", R"({"x": 1, "y": 1, "route": 2})");

  const json selection = call(state, "get_selection", "{}");

  REQUIRE(selection.at("target") == "waypoint");
  REQUIRE(selection.at("name") == "Route 2 · Waypoint 1");
  REQUIRE(selection.at("fields").size() == 5);
  REQUIRE(call(state, "get_state", "{}").at("waypoint_count") == 1);
}

TEST_CASE("list_waypoints reports each route in walking order, and who walks "
          "it") {
  EditorShellState state = stateWithAsset();
  (void)call(state, "add_waypoint", R"({"x": 5, "y": 1, "order": 2})");
  (void)call(state, "add_waypoint", R"({"x": 1, "y": 1, "order": 1})");
  (void)call(state, "place_asset", R"({"asset": "crate", "x": 0, "y": 0})");
  (void)call(state, "set_behavior",
             R"({"target": "selection", "behavior": "patrol", "route": 1})");

  const json listed = call(state, "list_waypoints", "{}");

  REQUIRE(listed.at("waypoints").size() == 2);
  REQUIRE(listed.at("route_count") == 9);
  const json& route = listed.at("routes").at(0);
  REQUIRE(route.at("route") == 1);
  REQUIRE(route.at("points").at(0).at("x") == Approx(1.0f));
  REQUIRE(route.at("points").at(1).at("x") == Approx(5.0f));
  REQUIRE(route.at("patrolled_by") ==
          json::array({state.document.placements[0].id}));
}

TEST_CASE("set_behavior names a route, takes it away with 0, and refuses one "
          "past nine") {
  EditorShellState state = stateWithAsset();
  (void)call(state, "place_asset", R"({"asset": "crate", "x": 0, "y": 0})");

  const json routed = call(
      state, "set_behavior",
      R"({"target": "placement", "index": 0, "behavior": "patrol", "route": 5})");
  REQUIRE(routed.at("route") == 5);
  REQUIRE(statusOf(state, "set_behavior",
                   R"({"target": "placement", "index": 0, "route": 10})") ==
          AgentStatus::BAD_PARAMS);
  const json cleared =
      call(state, "set_behavior",
           R"({"target": "placement", "index": 0, "route": 0})");
  REQUIRE(cleared.at("route") == 0);
  REQUIRE(state.history.actions.size() == 3);
}

TEST_CASE("adding a waypoint is refused while the level is played") {
  EditorShellState state = stateWithAsset();
  state.playtest.mode = EditorPlayMode::PLAYING;
  REQUIRE(statusOf(state, "add_waypoint", R"({"x": 1, "y": 1})") ==
          AgentStatus::UNAVAILABLE);
}
