#include <catch2/catch_test_macros.hpp>
#include <engine/audio/audio-volumes-json.h>

using eng::audio::AUDIO_BUS_COUNT;
using eng::audio::AudioBus;
using eng::audio::audioBusName;
using eng::audio::audioBusNamed;
using eng::audio::AudioMuting;
using eng::audio::AudioVolumes;
using eng::audio::busVolume;
using eng::audio::parseAudioVolumes;
using eng::audio::setBusVolume;
using eng::audio::writeAudioVolumes;

TEST_CASE("every bus has a name that finds it again") {
  for (uint8_t i = 0; i < AUDIO_BUS_COUNT; ++i) {
    const auto bus = static_cast<AudioBus>(i);
    REQUIRE(audioBusNamed(audioBusName(bus)) == bus);
  }
  REQUIRE_FALSE(audioBusNamed("voice").has_value());
}

TEST_CASE("volumes written are read back the same") {
  AudioVolumes volumes;
  volumes.master = 0.8F;
  setBusVolume(volumes, AudioBus::MUSIC, 0.25F);
  volumes.muting = AudioMuting::MUTED;
  const auto load = parseAudioVolumes(writeAudioVolumes(volumes));
  REQUIRE(load.problems.empty());
  REQUIRE(load.volumes.master == 0.8F);
  REQUIRE(busVolume(load.volumes, AudioBus::MUSIC) == 0.25F);
  REQUIRE(busVolume(load.volumes, AudioBus::EFFECTS) == 1.0F);
  REQUIRE(load.volumes.muting == AudioMuting::MUTED);
}

TEST_CASE("a file's gaps keep the defaults, and its range is clamped") {
  const auto load = parseAudioVolumes(R"({"music": 1.5, "effects": -2})");
  REQUIRE(load.problems.empty());
  REQUIRE(load.volumes.master == 1.0F);
  REQUIRE(busVolume(load.volumes, AudioBus::MUSIC) == 1.0F);
  REQUIRE(busVolume(load.volumes, AudioBus::EFFECTS) == 0.0F);
  REQUIRE(load.volumes.muting == AudioMuting::AUDIBLE);
}

TEST_CASE("what cannot be read is skipped and said, and the rest still loads") {
  const auto load = parseAudioVolumes(
      R"({"master": "loud", "muted": 1, "voice": 0.5, "music": 0.5})");
  REQUIRE(load.problems.size() == 3);
  REQUIRE(load.volumes.master == 1.0F);
  REQUIRE(busVolume(load.volumes, AudioBus::MUSIC) == 0.5F);
}

TEST_CASE("text that is not JSON gives the defaults and one problem") {
  const auto load = parseAudioVolumes("volume up");
  REQUIRE(load.problems.size() == 1);
  REQUIRE(load.volumes.master == 1.0F);
}
