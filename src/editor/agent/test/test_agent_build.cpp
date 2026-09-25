#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <game/logic/game-logic-entry.h>
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

/// A state with a project open at /games/dig.
EditorShellState withProject() {
  EditorShellState state;
  state.project.loaded = true;
  state.project.root = "/games/dig";
  return state;
}

}  // namespace

TEST_CASE("get_build reports a project with no logic and nothing built") {
  EditorShellState state = withProject();

  const json build = call(state, "get_build", "{}");

  CHECK(build["has_logic"] == false);
  CHECK(build["source"] == "/games/dig/src");
  CHECK(build["logic_loaded"] == false);
  CHECK(build["api_version"] == game::GAME_LOGIC_API_VERSION);
  CHECK(build["deployed"].is_null());
  CHECK(build["build"]["status"] == "idle");
  CHECK(build["toolchain"]["cmake"].is_string());
}

TEST_CASE("get_build reports a failed build with its errors") {
  EditorShellState state = withProject();
  state.build.kind = EditorBuildKind::LOGIC;
  state.build.status = EditorBuildStatus::FAILED;
  state.build.builds = 2;
  state.build.errors = {"game-logic.cpp:3:1: error: unknown type name"};

  const json build = call(state, "get_build", "{}")["build"];

  CHECK(build["kind"] == "logic");
  CHECK(build["status"] == "failed");
  CHECK(build["builds"] == 2);
  CHECK(build["errors"].size() == 1);
}

TEST_CASE("the build commands run through run_command once a project is "
          "open") {
  EditorShellState none;
  EditorShellState open = withProject();

  CHECK(runAgentTool(none, "run_command", R"({"command": "build_game_logic"})")
            .status != AgentStatus::OK);
  const AgentResult result =
      runAgentTool(open, "run_command", R"({"command": "deploy_game"})");
  CHECK(result.status == AgentStatus::OK);
  CHECK(result.host.command == EditorMenuCommand::DEPLOY_GAME);
}

TEST_CASE("a build command is refused while a build is running") {
  EditorShellState state = withProject();
  state.build.status = EditorBuildStatus::RUNNING;

  CHECK(runAgentTool(state, "run_command", R"({"command": "build_game_logic"})")
            .status != AgentStatus::OK);
}

TEST_CASE("new_game_logic is refused for a project that has logic") {
  EditorShellState state = withProject();
  state.build.has_logic = true;

  CHECK(runAgentTool(state, "run_command", R"({"command": "new_game_logic"})")
            .status != AgentStatus::OK);
}

TEST_CASE("get_playtest reports the game logic and how the run stands") {
  EditorShellState state;
  state.playtest.logic = true;
  state.playtest.outcome = game::RunOutcome::WON;
  state.playtest.logic_log = {"Cleared"};

  const json playtest = call(state, "get_playtest", "{}");

  CHECK(playtest["logic"] == true);
  CHECK(playtest["outcome"] == "won");
  CHECK(playtest["logic_log"] == json::array({"Cleared"}));
}

TEST_CASE("get_playtest marks the actors the logic spawned") {
  EditorShellState state;
  EditorPlaytestActor imp;
  imp.id = "imp";
  imp.spawned = true;
  state.playtest.actors.push_back(imp);

  const json actor = call(state, "get_playtest", "{}")["actors"][0];

  CHECK(actor["id"] == "imp");
  CHECK(actor["spawned"] == true);
}

TEST_CASE("create_project asks the editor to make a project where it is told") {
  EditorShellState state;

  const AgentResult result = runAgentTool(
      state, "create_project", R"({"path": "/games/dig", "name": "Dig"})");

  CHECK(result.status == AgentStatus::OK);
  CHECK(result.host.kind == AgentHostRequestKind::CREATE_PROJECT);
  CHECK(result.host.path == "/games/dig");
  CHECK(result.host.name == "Dig");
}

TEST_CASE("create_project needs a path") {
  EditorShellState state;

  CHECK(runAgentTool(state, "create_project", "{}").status ==
        AgentStatus::BAD_PARAMS);
}
