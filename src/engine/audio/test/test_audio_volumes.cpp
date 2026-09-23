#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/audio/audio-volumes.h>

using Catch::Approx;
using eng::audio::applyAudioVolumes;
using eng::audio::AudioBus;
using eng::audio::AudioClip;
using eng::audio::AudioEngine;
using eng::audio::AudioMuting;
using eng::audio::audioVolumeGain;
using eng::audio::AudioVolumes;
using eng::audio::busVolume;
using eng::audio::setBusVolume;

namespace {

/// What a flat 0.5 clip on @p bus renders to under @p volumes.
float heard(const AudioVolumes& volumes, AudioBus bus) {
  AudioEngine engine;
  const auto clip = engine.clips().add(
      "tone", AudioClip{.samples = std::vector<float>(64, 0.5F)});
  applyAudioVolumes(engine, volumes);
  (void)engine.play({.clip = clip, .bus = bus});
  std::vector<float> out(2);
  engine.render(out);
  return out[0];
}

}  // namespace

TEST_CASE("a slider's volume becomes a gain along a square") {
  REQUIRE(audioVolumeGain(1.0F) == 1.0F);
  REQUIRE(audioVolumeGain(0.5F) == 0.25F);
  REQUIRE(audioVolumeGain(0.0F) == 0.0F);
  REQUIRE(audioVolumeGain(-1.0F) == 0.0F);
  REQUIRE(audioVolumeGain(3.0F) == 1.0F);
}

TEST_CASE("a bus's volume is set clamped and read back") {
  AudioVolumes volumes;
  setBusVolume(volumes, AudioBus::MUSIC, 0.4F);
  setBusVolume(volumes, AudioBus::EFFECTS, 7.0F);
  REQUIRE(busVolume(volumes, AudioBus::MUSIC) == 0.4F);
  REQUIRE(busVolume(volumes, AudioBus::EFFECTS) == 1.0F);
  REQUIRE(busVolume(volumes, AudioBus::INTERFACE) == 1.0F);
}

TEST_CASE("applied volumes scale what is heard, bus by bus") {
  AudioVolumes volumes;
  REQUIRE(heard(volumes, AudioBus::EFFECTS) == Approx(0.5F));
  volumes.master = 0.5F;
  setBusVolume(volumes, AudioBus::MUSIC, 0.5F);
  REQUIRE(heard(volumes, AudioBus::EFFECTS) == Approx(0.125F));
  REQUIRE(heard(volumes, AudioBus::MUSIC) == Approx(0.03125F));
}

TEST_CASE("muting silences everything and keeps the volumes") {
  AudioVolumes volumes;
  volumes.master = 0.7F;
  volumes.muting = AudioMuting::MUTED;
  REQUIRE(heard(volumes, AudioBus::INTERFACE) == 0.0F);
  REQUIRE(volumes.master == 0.7F);
}
