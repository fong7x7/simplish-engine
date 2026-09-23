#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
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

/// The status one tool call ends in.
AgentStatus statusOf(EditorShellState& state, std::string_view tool,
                     std::string_view params) {
  return runAgentTool(state, tool, params).status;
}

}  // namespace

TEST_CASE("get_controls reports every action in the file's own words") {
  EditorShellState state;
  state.controls.file = "/tmp/input-bindings.json";
  const json controls = call(state, "get_controls", "{}");
  REQUIRE(controls["file"] == "/tmp/input-bindings.json");
  REQUIRE(controls["actions"]["move_up"].size() == 4);
  REQUIRE(controls["actions"]["fire"][0] == "pad:right_trigger");
  REQUIRE(controls["deadzones"]["left_stick"] == 0.2);
}

TEST_CASE("set_controls gives an action exactly what it lists") {
  EditorShellState state;
  const json after =
      call(state, "set_controls",
           R"({"action": "fire", "controls": "pad:south, key:space"})");
  REQUIRE(after["actions"]["fire"] == json::array({"pad:south", "key:space"}));
  REQUIRE(state.controls.revision == 1);
  REQUIRE(
      state.controls.bindings.actionsFor(input::InputSource::key(' ')).size() ==
      1);
}

TEST_CASE("set_controls with an empty list unbinds the action") {
  EditorShellState state;
  const json after =
      call(state, "set_controls", R"({"action": "aim_up", "controls": ""})");
  REQUIRE(after["actions"]["aim_up"].empty());
}

TEST_CASE("set_controls refuses what is not there and changes nothing") {
  EditorShellState state;
  REQUIRE(statusOf(state, "set_controls",
                   R"({"action": "jump", "controls": "key:j"})") ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(statusOf(state, "set_controls",
                   R"({"action": "fire", "controls": "pad:banana"})") ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(statusOf(state, "set_controls", R"({"action": "fire"})") ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(state.controls.revision == 0);
  REQUIRE(state.controls.bindings.sources(input::InputAction::FIRE).size() ==
          2);
}

TEST_CASE("set_controls tunes the deadzones and resets to the defaults") {
  EditorShellState state;
  (void)call(state, "set_controls",
             R"({"action": "fire", "controls": "", "trigger": 0.4})");
  const json reset = call(state, "set_controls", R"({"reset": true})");
  REQUIRE(reset["actions"]["fire"].size() == 2);
  // A reset keeps the deadzones, as the Controls screen's does.
  REQUIRE(reset["deadzones"]["trigger"].get<float>() == 0.4F);
}
