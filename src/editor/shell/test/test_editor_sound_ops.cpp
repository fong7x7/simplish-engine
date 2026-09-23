#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <editor/shell/editor-sound-ops.h>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace eng;
using namespace eng::editor;
namespace fs = std::filesystem;

namespace {

/// Write a mono 16-bit WAV of @p frames samples at @p value to @p path.
void writeWav(const fs::path& path, size_t frames, int16_t value) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  const auto le = [&out](uint32_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) {
      out.put(static_cast<char>((v >> (8 * i)) & 0xFFU));
    }
  };
  const auto data = static_cast<uint32_t>(frames * 2);
  out << "RIFF";
  le(36 + data, 4);
  out << "WAVEfmt ";
  le(16, 4), le(1, 2), le(1, 2), le(48000, 4), le(96000, 4), le(2, 2);
  le(16, 2);
  out << "data";
  le(data, 4);
  for (size_t i = 0; i < frames; ++i) {
    le(static_cast<uint16_t>(value), 2);
  }
}

}  // namespace

TEST_CASE("every combat cue has a slot with a name of its own") {
  const std::vector<std::string> slots = editorSoundSlots();
  // Then every step set's footstep on every surface.
  REQUIRE(slots.size() ==
          game::COMBAT_CUE_KIND_COUNT +
              game::STEP_SET_COUNT * game::FOOTSTEP_SURFACE_COUNT);
  REQUIRE(slots.front() == "combat.shot_fired");
  REQUIRE(editorSoundSlotLabel("combat.blast") == "Blast");
  REQUIRE(editorSoundSlotLabel("combat.shot_hit_wall") == "Shot hits a wall");
  REQUIRE(editorSoundSlotLabel("ambience.rain") == "ambience.rain");
}

TEST_CASE("a slot is found by its bank name or by its cue alone") {
  REQUIRE(findEditorSoundSlot("combat.blast") == "combat.blast");
  REQUIRE(findEditorSoundSlot("blast") == "combat.blast");
  REQUIRE_FALSE(findEditorSoundSlot("footstep").has_value());
  REQUIRE_FALSE(findEditorSoundSlot("").has_value());
}

TEST_CASE("assigning a file adds, replaces, and an empty one removes") {
  EditorSoundTable table;
  assignEditorSound(table, "combat.blast", "a.wav");
  assignEditorSound(table, "combat.blast", "b.wav");
  REQUIRE(table.sounds.size() == 1);
  REQUIRE(editorAssignedSound(table, "combat.blast") == fs::path("b.wav"));
  assignEditorSound(table, "combat.blast", {});
  REQUIRE(table.sounds.empty());
  REQUIRE(editorAssignedSound(table, "combat.blast").empty());
}

TEST_CASE("stepping goes built-in, then each file, and wraps round") {
  const std::vector<fs::path> files{"a.wav", "b.ogg"};
  REQUIRE(editorStepSoundFile({}, files, 1) == fs::path("a.wav"));
  REQUIRE(editorStepSoundFile("a.wav", files, 1) == fs::path("b.ogg"));
  REQUIRE(editorStepSoundFile("b.ogg", files, 1).empty());
  REQUIRE(editorStepSoundFile({}, files, -1) == fs::path("b.ogg"));
  REQUIRE(editorStepSoundFile("gone.wav", files, 1) == fs::path("a.wav"));
  REQUIRE(editorStepSoundFile({}, {}, 1).empty());
}

TEST_CASE("a project's file plays in its slot, keeping the built-in's id") {
  const fs::path assets = fs::temp_directory_path() / "simplish-sound-ops";
  writeWav(assets / "boom.wav", 100, 16384);
  audio::AudioClipBank bank;
  EditorSoundTable table;
  table.sounds = {{"combat.blast", "boom.wav"}};
  const EditorSoundLoad load = loadEditorSounds(bank, table, assets, 48000);
  REQUIRE(load.problems.empty());
  const audio::AudioClip* blast = bank.clip(load.clips.back());
  REQUIRE(bank.find("combat.blast") == load.clips.back());
  REQUIRE(blast->samples.size() == 100);
  REQUIRE(blast->samples[0] == 0.5F);
  fs::remove_all(assets);
}

TEST_CASE("a file that will not play leaves the built-in, and says so") {
  audio::AudioClipBank bank;
  EditorSoundTable table;
  table.sounds = {{"combat.blast", "nowhere.wav"}, {"footstep", "a.wav"}};
  const EditorSoundLoad load =
      loadEditorSounds(bank, table, "/nonexistent", 48000);
  REQUIRE(load.problems.size() == 2);
  REQUIRE(bank.clip(load.clips.back())->samples.size() > 100);
}

TEST_CASE("a footstep slot is labelled by its feet and what they are on") {
  REQUIRE(editorSoundSlotLabel("step.boots.sand") == "Boots on sand");
  REQUIRE(findEditorSoundSlot("step.claws.metal") == "step.claws.metal");
  REQUIRE(editorSoundGroupHeading("step.heavy.ground") == "Footsteps — Heavy");
  REQUIRE_FALSE(editorSoundGroupHeading("step.heavy.sand").has_value());
}

TEST_CASE("the built-in footsteps load with the combat sounds, and every "
          "step set borrows them") {
  audio::AudioClipBank bank;
  const EditorSoundLoad load = loadEditorSounds(bank, {}, {}, 48000);
  REQUIRE(bank.find("step.default.wood").has_value());
  const game::FootstepClip& claws =
      load.footsteps[static_cast<size_t>(game::StepSet::CLAWS)]
                    [static_cast<size_t>(game::FootstepSurface::WOOD)];
  REQUIRE(claws.clip == *bank.find("step.default.wood"));
  REQUIRE(claws.source == game::FootstepClipSource::BORROWED);
  REQUIRE(load.loaded.empty());
}
