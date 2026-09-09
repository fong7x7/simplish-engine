#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <editor/agent/agent-state-json.h>
#include <nlohmann/json.hpp>

using nlohmann::json;
using namespace eng;
using namespace eng::editor;

TEST_CASE("a tool nobody has is named as unknown, not guessed at") {
  EditorShellState state;

  const AgentResult result = runAgentTool(state, "delete_everything", "{}");

  REQUIRE(result.status == AgentStatus::UNKNOWN_TOOL);
  REQUIRE_FALSE(result.changed);
}

TEST_CASE("params that are not an object are refused") {
  EditorShellState state;

  REQUIRE(runAgentTool(state, "get_state", "[1, 2]").status ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(runAgentTool(state, "get_state", "not json").status ==
          AgentStatus::BAD_PARAMS);
}

TEST_CASE("a tool that takes nothing may be called with nothing at all") {
  EditorShellState state;

  REQUIRE(runAgentTool(state, "get_state", "").status == AgentStatus::OK);
}

TEST_CASE("a request names its tool and carries its params") {
  EditorShellState state;

  const AgentResult result = runAgentRequest(state, R"({"tool": "add_light",
                 "params": {"kind": "point", "x": 1, "y": 2}})");

  REQUIRE(result.status == AgentStatus::OK);
  REQUIRE(result.changed);
  REQUIRE(state.document.lights.size() == 1);
}

TEST_CASE("a request with no tool in it is a bad call") {
  EditorShellState state;

  REQUIRE(runAgentRequest(state, R"({"params": {}})").status ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(runAgentRequest(state, "[]").status == AgentStatus::BAD_PARAMS);
}

TEST_CASE("a tool taking no params may be requested without a params key") {
  EditorShellState state;

  REQUIRE(runAgentRequest(state, R"({"tool": "list_lights"})").status ==
          AgentStatus::OK);
}

TEST_CASE("a response carries the status word, the payload, and the change") {
  EditorShellState state;
  const AgentResult ok = runAgentTool(state, "list_lights", "{}");
  const AgentResult bad = runAgentTool(state, "nope", "{}");

  const json good = json::parse(agentResponseJson(ok));
  REQUIRE(good.at("status") == "ok");
  REQUIRE(good.at("changed") == false);
  REQUIRE(good.at("result").at("lights").empty());

  const json failed = json::parse(agentResponseJson(bad));
  REQUIRE(failed.at("status") == "unknown_tool");
  // The message explains the call, so a caller does not have to guess.
  REQUIRE_FALSE(failed.at("result").at("message").get<std::string>().empty());
}
