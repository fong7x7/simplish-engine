#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;
using namespace eng;
using namespace eng::editor;
namespace fs = std::filesystem;

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

/// A shell state with a project open at @p root, holding one sound file.
EditorShellState withProject(const fs::path& root) {
  EditorShellState state;
  state.project.loaded = true;
  state.project.root = root;
  state.sound_files = {"sounds/boom.wav"};
  return state;
}

/// Write a tiny valid mono 16-bit WAV to @p path.
void writeWav(const fs::path& path) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  const unsigned char bytes[] = {
      'R',  'I',  'F', 'F', 40, 0,    0, 0, 'W', 'A',  'V', 'E',
      'f',  'm',  't', ' ', 16, 0,    0, 0, 1,   0,    1,   0,
      0x80, 0xBB, 0,   0,   0,  0x77, 1, 0, 2,   0,    16,  0,
      'd',  'a',  't', 'a', 4,  0,    0, 0, 0,   0x40, 0,   0x40};
  out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

}  // namespace

TEST_CASE("get_sound reports the volumes and every slot") {
  EditorShellState state = withProject("/tmp/game");
  state.sound.file = "/tmp/audio-volumes.json";
  state.sounds.sounds = {{"combat.blast", "sounds/gone.wav"}};
  const json sound = call(state, "get_sound", "{}");
  REQUIRE(sound["volumes"]["master"] == 1.0);
  REQUIRE(sound["volumes"]["muted"] == false);
  REQUIRE(sound["volumes"]["file"] == "/tmp/audio-volumes.json");
  // Four combat sounds, then forty-five footsteps.
  REQUIRE(sound["slots"].size() == 49);
  REQUIRE(sound["slots"][0]["file"].is_null());
  REQUIRE(sound["slots"][3]["file"] == "sounds/gone.wav");
  REQUIRE(sound["slots"][3]["missing"] == true);
  REQUIRE(sound["sound_files"][0] == "sounds/boom.wav");
}

TEST_CASE("set_volume sets what it is given and keeps the rest") {
  EditorShellState state;
  const json sound =
      call(state, "set_volume", R"({"music": 0.25, "muted": true})");
  REQUIRE(sound["volumes"]["music"] == 0.25);
  REQUIRE(sound["volumes"]["effects"] == 1.0);
  REQUIRE(state.sound.volumes.muting == audio::AudioMuting::MUTED);
  REQUIRE(state.sound.revision == 1);
}

TEST_CASE("set_volume refuses a volume out of range, changing nothing") {
  EditorShellState state;
  REQUIRE(statusOf(state, "set_volume", R"({"master": 0.5, "music": 2})") ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(statusOf(state, "set_volume", R"({"effects": "loud"})") ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(state.sound.volumes.master == 1.0F);
  REQUIRE(state.sound.revision == 0);
}

TEST_CASE("set_sound plays a project file in a slot, and back again") {
  EditorShellState state = withProject("/tmp/game");
  call(state, "set_sound", R"({"slot": "blast", "file": "sounds/boom.wav"})");
  REQUIRE(state.sounds.sounds.size() == 1);
  REQUIRE(state.sounds.sounds[0].slot == "combat.blast");
  REQUIRE(state.sounds.revision == 1);
  call(state, "set_sound", R"({"slot": "combat.blast", "file": ""})");
  REQUIRE(state.sounds.sounds.empty());
}

TEST_CASE("set_sound refuses what it cannot play") {
  EditorShellState state = withProject("/tmp/game");
  REQUIRE(statusOf(state, "set_sound", R"({"slot": "moo", "file": ""})") ==
          AgentStatus::NOT_FOUND);
  REQUIRE(
      statusOf(state, "set_sound", R"({"slot": "blast", "file": "x.wav"})") ==
      AgentStatus::NOT_FOUND);
  EditorShellState closed;
  REQUIRE(statusOf(closed, "set_sound", R"({"slot": "blast", "file": ""})") ==
          AgentStatus::UNAVAILABLE);
}

TEST_CASE("import_sound copies a file in and can play it in a slot") {
  const fs::path root = fs::temp_directory_path() / "simplish-agent-import";
  fs::remove_all(root);
  writeWav(root / "outside.wav");
  EditorShellState state = withProject(root);
  const json sound =
      call(state, "import_sound",
           json{{"path", (root / "outside.wav").string()}, {"slot", "blast"}}
               .dump());
  REQUIRE(sound["sound_files"].size() == 1);
  REQUIRE(sound["sound_files"][0] == "sounds/outside.wav");
  REQUIRE(sound["slots"][3]["file"] == "sounds/outside.wav");
  REQUIRE(fs::exists(root / "assets" / "sounds" / "outside.wav"));
  fs::remove_all(root);
}

TEST_CASE("import_sound refuses a file it cannot play, or a bad slot") {
  EditorShellState state = withProject("/tmp/game");
  REQUIRE(statusOf(state, "import_sound", R"({"path": "/nowhere.wav"})") ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(statusOf(state, "import_sound",
                   R"({"path": "/nowhere.wav", "slot": "moo"})") ==
          AgentStatus::NOT_FOUND);
  REQUIRE(statusOf(state, "import_sound", "{}") == AgentStatus::BAD_PARAMS);
}

TEST_CASE("play_sound asks the editor to play a slot or a project file") {
  EditorShellState state = withProject("/tmp/game");
  const AgentResult slot =
      runAgentTool(state, "play_sound", R"({"name": "blast"})");
  REQUIRE(slot.host.kind == AgentHostRequestKind::PLAY_SOUND);
  REQUIRE(slot.host.sound == "blast");
  REQUIRE(runAgentTool(state, "play_sound", R"({"name": "sounds/boom.wav"})")
              .status == AgentStatus::OK);
  REQUIRE(statusOf(state, "play_sound", R"({"name": "nope.wav"})") ==
          AgentStatus::NOT_FOUND);
  REQUIRE(statusOf(state, "play_sound", R"({"name": ""})") ==
          AgentStatus::NOT_FOUND);
}
