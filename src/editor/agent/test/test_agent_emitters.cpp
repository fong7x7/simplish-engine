#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-effect-shot.h>
#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;
using namespace eng::editor;

namespace {

/// A project with nothing in its level yet.
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

/// The status one tool call ends in.
AgentStatus statusOf(EditorShellState& state, std::string_view tool,
                     std::string_view params) {
  return runAgentTool(state, tool, params).status;
}

}  // namespace

TEST_CASE("add_emitter places sparks at chest height and selects them") {
  EditorShellState state = openProject();
  const json added = call(state, "add_emitter", R"({"x": 2.5, "y": 3.5})");

  REQUIRE(added.at("id") == "emitter_01");
  REQUIRE(added.at("ref") == "emitter:emitter_01");
  REQUIRE(added.at("effect") == "wall_sparks");
  REQUIRE(added.at("is_preset") == true);
  REQUIRE(added.at("position").at("z") == 0.9f);
  REQUIRE(added.at("properties").contains("particles"));
  REQUIRE(state.selection.kind == EditorSelectionKind::EMITTER);
  REQUIRE(canUndoEditorAction(state.history));
}

TEST_CASE("add_emitter starts from the preset it is given, and no other") {
  EditorShellState state = openProject();
  const json smoke =
      call(state, "add_emitter", R"({"x": 0, "y": 0, "effect": "smoke"})");
  REQUIRE(smoke.at("effect") == "smoke");

  REQUIRE(statusOf(state, "add_emitter",
                   R"({"x": 0, "y": 0, "effect": "plasma"})") ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(statusOf(state, "add_emitter", R"({"y": 0})") ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(state.document.emitters.size() == 1);
}

TEST_CASE("set_property changes an emitter's burst, and undo takes it back") {
  EditorShellState state = openProject();
  (void)call(state, "add_emitter", R"({"x": 0, "y": 0})");
  const json more = call(
      state, "set_property",
      R"({"target": "emitter", "index": 0, "field": "particles", "value": 30})");

  REQUIRE(more.at("properties").at("particles") == 30.0f);
  REQUIRE(more.at("is_preset") == false);
  (void)call(state, "undo", "{}");
  REQUIRE(state.document.emitters[0].burst.count != 30);
}

TEST_CASE("set_effect starts the selected emitter from another preset") {
  EditorShellState state = openProject();
  (void)call(state, "add_emitter", R"({"x": 0, "y": 0})");
  const json fire = call(state, "set_effect",
                         R"({"target": "selection", "effect": "fireball"})");

  REQUIRE(fire.at("effect") == "fireball");
  REQUIRE(fire.at("is_preset") == true);
  REQUIRE(statusOf(state, "set_effect",
                   R"({"target": "emitter", "index": 0, "effect": "nope"})") ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(statusOf(state, "set_effect",
                   R"({"target": "emitter", "index": 4, "effect": "smoke"})") ==
          AgentStatus::BAD_PARAMS);
}

TEST_CASE("an emitter is moved, selected and deleted as any entry is") {
  EditorShellState state = openProject();
  (void)call(state, "add_emitter", R"({"x": 1.5, "y": 1.5})");
  const json moved =
      call(state, "translate", R"({"target": "emitter", "index": 0, "dx": 2})");
  REQUIRE(moved.at("position").at("x") == 3.5f);

  (void)call(state, "select", R"({"target": "none"})");
  (void)call(state, "select", R"({"target": "emitter", "index": 0})");
  REQUIRE(state.selection.kind == EditorSelectionKind::EMITTER);
  const json removed =
      call(state, "delete", R"({"target": "emitter", "index": 0})");
  REQUIRE(removed.at("removed") == true);
  REQUIRE(state.document.emitters.empty());
}

TEST_CASE("list_emitters lists every emitter and every preset") {
  EditorShellState state = openProject();
  (void)call(state, "add_emitter", R"({"x": 0, "y": 0})");
  (void)call(state, "add_emitter", R"({"x": 1, "y": 0, "effect": "embers"})");
  const json listed = call(state, "list_emitters", "{}");

  REQUIRE(listed.at("emitters").size() == 2);
  REQUIRE(listed.at("emitters").at(1).at("index") == 1);
  REQUIRE(listed.at("effects").size() == 8);
  REQUIRE(listed.at("effects").at(0).at("id") == "muzzle_flash");
  const json selected = call(state, "get_selection", "{}");
  REQUIRE(selected.at("target") == "emitter");
  REQUIRE(selected.at("fields").size() == 29);
  REQUIRE(call(state, "get_state", "{}").at("emitter_count") == 2);
}

TEST_CASE("play_effect hands the editor a preset's one burst, pointing up") {
  EditorShellState state = openProject();
  const AgentResult result = runAgentTool(
      state, "play_effect", R"({"effect": "embers", "x": 2, "y": 3})");

  REQUIRE(result.status == AgentStatus::OK);
  REQUIRE_FALSE(result.changed);
  REQUIRE(result.host.kind == AgentHostRequestKind::PLAY_EFFECT);
  const EditorEffectShot& shot = result.host.effect;
  REQUIRE(shot.bursts.size() == 1);
  REQUIRE(shot.emit.at.x == 2.0f);
  REQUIRE(shot.emit.at.z == 0.9f);
  REQUIRE(shot.emit.direction.z == 1.0f);
  REQUIRE(json::parse(result.json).at("into") == "editor");
}

TEST_CASE("play_effect plays a whole combat effect: a blast on the floor") {
  EditorShellState state = openProject();
  const AgentResult result =
      runAgentTool(state, "play_effect",
                   R"({"effect": "blast", "x": 0, "y": 0, "scale": 2})");

  REQUIRE(result.status == AgentStatus::OK);
  const EditorEffectShot& shot = result.host.effect;
  REQUIRE(shot.bursts.size() == 3);
  REQUIRE(shot.emit.at.z == 0.0f);
  REQUIRE(shot.emit.scale == 2.0f);
  REQUIRE(shot.flash.intensity > 0.0f);
}

TEST_CASE("play_effect fires an emitter's own burst, edits and all") {
  EditorShellState state = openProject();
  (void)call(state, "add_emitter", R"({"x": 5.5, "y": 1.5})");
  (void)call(state, "set_property",
             R"({"target": "selection", "field": "particles", "value": 77})");
  const AgentResult result =
      runAgentTool(state, "play_effect", R"({"emitter": 0})");

  REQUIRE(result.status == AgentStatus::OK);
  REQUIRE(result.host.effect.bursts.at(0).count == 77);
  REQUIRE(result.host.effect.emit.at.x == 5.5f);
  REQUIRE(json::parse(result.json).at("particles") == 77);
}

TEST_CASE("play_effect is refused what it cannot play, and not while playing") {
  EditorShellState state = openProject();
  REQUIRE(
      statusOf(state, "play_effect", R"({"effect": "nope", "x": 0, "y": 0})") ==
      AgentStatus::BAD_PARAMS);
  REQUIRE(statusOf(state, "play_effect", R"({"effect": "smoke"})") ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(statusOf(state, "play_effect", R"({"emitter": 3})") ==
          AgentStatus::BAD_PARAMS);
  // Not an edit, so a running playtest takes it — into its own effects.
  state.playtest.mode = EditorPlayMode::PLAYING;
  const json played =
      call(state, "play_effect", R"({"effect": "shot_fired", "x": 0, "y": 0})");
  REQUIRE(played.at("into") == "playtest");
}

TEST_CASE("get_effects reports the viewport's effects and each emitter's "
          "bursts") {
  EditorShellState state = openProject();
  (void)call(state, "add_emitter", R"({"x": 0, "y": 0})");
  state.effects.particles = 30;
  state.effects.lights = 1;
  state.effects.emitter_bursts = {4};
  state.effects.shots_played = 2;
  const json effects = call(state, "get_effects", "{}");

  REQUIRE(effects.at("source") == "editor");
  REQUIRE(effects.at("particles") == 30);
  REQUIRE(effects.at("lights") == 1);
  REQUIRE(effects.at("shots_played") == 2);
  REQUIRE(effects.at("emitters").at(0).at("bursts") == 4);
  REQUIRE(effects.at("emitters").at(0).at("id") == "emitter_01");
}
