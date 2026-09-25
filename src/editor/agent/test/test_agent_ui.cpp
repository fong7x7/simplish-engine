#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <game/ui/ui-screen-json.h>
#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;
using namespace eng;
using namespace eng::editor;

namespace {

/// A shell state with a project open, holding a pause menu with two
/// buttons.
EditorShellState withPauseMenu() {
  EditorShellState state;
  state.project.loaded = true;
  state.project.root = "/tmp/game";
  state.ui.screens.push_back(*game::parseUiScreen(R"({"root": {
      "type": "panel", "children": [
        {"type": "button", "text": "Resume", "action": "resume", "id": "go"},
        {"type": "button", "text": "Quit", "action": "quit"}]}})",
                                                  "pause")
                                  .screen);
  state.ui.problems = {"broken.ui.json: not a JSON object"};
  return state;
}

/// Run one tool, requiring it to succeed; its result.
AgentResult ok(EditorShellState& state, std::string_view tool,
               std::string_view params) {
  const AgentResult result = runAgentTool(state, tool, params);
  INFO("tool " << tool << " said " << result.json);
  REQUIRE(result.status == AgentStatus::OK);
  return result;
}

}  // namespace

TEST_CASE("get_ui_screens lists each screen's buttons, the actions and the "
          "problems") {
  EditorShellState state = withPauseMenu();

  const json read = json::parse(ok(state, "get_ui_screens", "{}").json);

  REQUIRE(read["screens"].size() == 1);
  CHECK(read["screens"][0]["layer"] == "menu");
  CHECK(read["screens"][0]["buttons"][0]["id"] == "go");
  CHECK(read["actions"] == json::array({"quit", "resume"}));
  CHECK(read["problems"].size() == 1);
}

TEST_CASE("set_ui_screen checks a screen, and leaves the writing to the "
          "editor") {
  EditorShellState state = withPauseMenu();

  const AgentResult written = ok(state, "set_ui_screen", R"({"id": "hud",
      "screen": {"layer": "hud", "root": {"type": "label", "text": "{score}"}}})");
  const AgentResult rootless =
      runAgentTool(state, "set_ui_screen", R"({"id": "hud", "screen": {}})");
  const AgentResult badly_named =
      runAgentTool(state, "set_ui_screen",
                   R"({"id": "../x", "screen": {"root": {"type": "panel"}}})");

  CHECK(written.host.kind == AgentHostRequestKind::WRITE_UI_SCREEN);
  CHECK(json::parse(written.host.text)["layer"] == "hud");
  CHECK(rootless.status == AgentStatus::BAD_PARAMS);
  CHECK(json::parse(rootless.json)["message"].get<std::string>().find(
            "root: a screen needs a root node") != std::string::npos);
  CHECK(badly_named.status == AgentStatus::BAD_PARAMS);
}

TEST_CASE("render_ui_screen asks the editor to draw a screen it has") {
  EditorShellState state = withPauseMenu();

  const AgentResult render = ok(state, "render_ui_screen",
                                R"({"id": "pause", "width": 99999,
                                    "values": {"score": 3}})");

  CHECK(render.host.kind == AgentHostRequestKind::RENDER_UI_SCREEN);
  CHECK(render.host.width == 4096);
  CHECK(render.host.height == 720);
  CHECK(json::parse(render.host.text)["score"] == 3);
  CHECK(runAgentTool(state, "render_ui_screen", R"({"id": "shop"})").status ==
        AgentStatus::NOT_FOUND);
}

TEST_CASE("press_ui queues a choice while playing, and only an action there "
          "is") {
  EditorShellState state = withPauseMenu();
  CHECK(runAgentTool(state, "press_ui", R"({"action": "quit"})").status ==
        AgentStatus::UNAVAILABLE);
  state.playtest.mode = EditorPlayMode::PLAYING;

  (void)ok(state, "press_ui", R"({"action": "quit"})");

  CHECK(state.playtest.ui.press == "quit");
  CHECK(runAgentTool(state, "press_ui", R"({"action": "jump"})").status ==
        AgentStatus::NOT_FOUND);
}

TEST_CASE("get_playtest reports the game screens shown and their buttons") {
  EditorShellState state = withPauseMenu();
  state.playtest.mode = EditorPlayMode::PLAYING;
  state.playtest.ui.open = {"pause"};
  state.playtest.ui.values = {{"score", "4"}};
  state.playtest.ui.buttons = {
      {"pause", "go", "resume", "Resume", {10.0F, 20.0F, 100.0F, 30.0F}}};

  const json playtest = json::parse(ok(state, "get_playtest", "{}").json);

  CHECK(playtest["ui"]["open"] == json::array({"pause"}));
  CHECK(playtest["ui"]["values"]["score"] == "4");
  CHECK(playtest["ui"]["buttons"][0]["rect"][2] == 100.0);
}
