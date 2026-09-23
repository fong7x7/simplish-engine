#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-audio-volumes.h>
#include <filesystem>
#include <fstream>

using namespace eng;
using namespace eng::editor;
namespace fs = std::filesystem;

TEST_CASE("with no file yet, full volume is written for the user to edit") {
  const fs::path file =
      fs::temp_directory_path() / "simplish-volumes" / "audio-volumes.json";
  fs::remove_all(file.parent_path());
  const audio::AudioVolumes volumes = loadEditorAudioVolumes(file);
  REQUIRE(volumes.master == 1.0F);
  REQUIRE(fs::exists(file));
  fs::remove_all(file.parent_path());
}

TEST_CASE("volumes saved are loaded back") {
  const fs::path file =
      fs::temp_directory_path() / "simplish-volumes-2" / "audio-volumes.json";
  audio::AudioVolumes volumes;
  volumes.master = 0.6F;
  audio::setBusVolume(volumes, audio::AudioBus::MUSIC, 0.2F);
  volumes.muting = audio::AudioMuting::MUTED;
  REQUIRE(saveEditorAudioVolumes(file, volumes));
  const audio::AudioVolumes read = loadEditorAudioVolumes(file);
  REQUIRE(read.master == 0.6F);
  REQUIRE(audio::busVolume(read, audio::AudioBus::MUSIC) == 0.2F);
  REQUIRE(read.muting == audio::AudioMuting::MUTED);
  fs::remove_all(file.parent_path());
}

TEST_CASE("a broken file gives full volume, and nowhere keeps nothing") {
  const fs::path file = fs::temp_directory_path() / "simplish-volumes-3.json";
  std::ofstream(file) << "{ not json";
  REQUIRE(loadEditorAudioVolumes(file).master == 1.0F);
  fs::remove(file);
  REQUIRE(loadEditorAudioVolumes({}).master == 1.0F);
  REQUIRE_FALSE(saveEditorAudioVolumes({}, {}));
}
