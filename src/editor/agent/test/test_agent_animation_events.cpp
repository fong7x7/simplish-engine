#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;
using namespace eng;
using namespace eng::editor;

namespace {

/// A project with a rigged knight, loaded, walking and idling, a sheet,
/// and a sound file.
EditorShellState withKnight() {
  EditorShellState state;
  state.project.loaded = true;
  EditorAsset knight;
  knight.name = "knight";
  knight.id = "characters_knight";
  knight.path = "/p/assets/characters/knight.glb";
  auto rig = std::make_shared<animation::Rig>();
  rig->clips = {{"walk", 1.0F, {}}, {"idle", 2.0F, {}}};
  knight.rig = rig;
  state.assets.push_back(knight);
  state.sheets.push_back("sprites/torch.png");
  state.sound_files.push_back("sounds/clank.wav");
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

TEST_CASE("every loaded clip is listed with where its events come from") {
  EditorShellState state = withKnight();
  const json listed = call(state, "list_animation_events", "{}");
  REQUIRE(listed.at("clips").size() == 2);
  REQUIRE(listed.at("clips")[0].at("asset") == "mesh:characters_knight");
  // This test rig has no feet, so nothing is detected.
  REQUIRE(listed.at("clips")[0].at("source") == "none");
}

TEST_CASE("a clip's events are written, heard as authored, and taken away") {
  EditorShellState state = withKnight();
  const json written = call(state, "set_animation_events",
                            R"({"asset": "knight", "clip": "walk",
                                "events": [{"at": 0.4, "sound": "footstep"},
                                           {"at": 0.9, "sound": "sounds/clank.wav",
                                            "gain": 0.5}]})");
  REQUIRE(written.at("clips")[0].at("source") == "authored");
  REQUIRE(written.at("clips")[0].at("events").size() == 2);
  REQUIRE(state.animation_events.clips.size() == 1);
  REQUIRE(state.animation_events.revision == 1);

  (void)call(state, "set_animation_events",
             R"({"asset": "mesh:characters_knight", "clip": "walk"})");
  REQUIRE(state.animation_events.clips.empty());
}

TEST_CASE("a sheet's frame events are written") {
  EditorShellState state = withKnight();
  const json written = call(state, "set_animation_events",
                            R"({"sheet": "sprites/torch.png",
                                "events": [{"frame": 1, "sound": "combat.blast"}]})");
  REQUIRE(written.at("sheets")[0].at("events")[0].at("frame") == 1);
}

TEST_CASE("an unknown clip, sheet or sound is refused and changes nothing") {
  EditorShellState state = withKnight();
  REQUIRE(runAgentTool(state, "set_animation_events",
                       R"({"asset": "knight", "clip": "dance", "events": []})")
              .status == AgentStatus::NOT_FOUND);
  REQUIRE(runAgentTool(state, "set_animation_events",
                       R"({"sheet": "nope.png", "events": []})")
              .status == AgentStatus::NOT_FOUND);
  REQUIRE(runAgentTool(state, "set_animation_events",
                       R"({"asset": "knight", "clip": "walk",
                           "events": [{"at": 0.1, "sound": "sounds/gone.wav"}]})")
              .status == AgentStatus::BAD_PARAMS);
  REQUIRE(state.animation_events.clips.empty());
  REQUIRE(state.animation_events.revision == 0);
}

TEST_CASE("a refused write leaves the row that was there") {
  EditorShellState state = withKnight();
  (void)call(state, "set_animation_events",
             R"({"asset": "knight", "clip": "walk",
                 "events": [{"at": 0.4, "sound": "footstep"}]})");
  REQUIRE(runAgentTool(state, "set_animation_events",
                       R"({"asset": "knight", "clip": "walk",
                           "events": [{"at": -1, "sound": "footstep"}]})")
              .status == AgentStatus::BAD_PARAMS);
  REQUIRE(state.animation_events.clips.size() == 1);
  REQUIRE(state.animation_events.clips[0].events[0].at == 0.4F);
}

TEST_CASE("an event past the end of its clip is refused") {
  EditorShellState state = withKnight();
  REQUIRE(runAgentTool(state, "set_animation_events",
                       R"({"asset": "knight", "clip": "walk",
                           "events": [{"at": 1.5, "sound": "footstep"}]})")
              .status == AgentStatus::BAD_PARAMS);
  REQUIRE(state.animation_events.clips.empty());
}
