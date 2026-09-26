#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <editor/shell/editor-shell-state.h>
#include <nlohmann/json.hpp>

using namespace eng::editor;

TEST_CASE("set_interface_size sets the user's scale and asks for a save") {
  EditorShellState state;
  const uint64_t before = state.graphics.revision;
  const AgentResult result =
      runAgentTool(state, "set_interface_size", R"({"scale": 1.25})");
  REQUIRE(result.status == AgentStatus::OK);
  CHECK(state.graphics.ui_scale == 1.25f);
  CHECK(state.graphics.revision == before + 1);
  CHECK(nlohmann::json::parse(result.json)["interface_scale"] == 1.25);
}

TEST_CASE("set_interface_size refuses a scale out of range") {
  EditorShellState state;
  CHECK(runAgentTool(state, "set_interface_size", R"({"scale": 4})").status ==
        AgentStatus::BAD_PARAMS);
  CHECK(runAgentTool(state, "set_interface_size", "{}").status ==
        AgentStatus::BAD_PARAMS);
  CHECK(state.graphics.ui_scale == 1.0f);
}

TEST_CASE("get_state reports the interface scale") {
  EditorShellState state;
  state.graphics.ui_scale = 1.5f;
  const AgentResult result = runAgentTool(state, "get_state", "{}");
  CHECK(nlohmann::json::parse(result.json)["interface_scale"] == 1.5);
}
