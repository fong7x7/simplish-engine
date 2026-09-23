#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/audio/audio-engine.h>

using Catch::Approx;
using eng::audio::AUDIO_COMMAND_CAPACITY;
using eng::audio::AudioBus;
using eng::audio::AudioClip;
using eng::audio::AudioClipId;
using eng::audio::AudioEngine;
using eng::audio::SoundId;

namespace {
AudioClip flat(float value, size_t frames) {
  return {.samples = std::vector<float>(frames, value), .channels = 1};
}

std::vector<float> render(AudioEngine& engine, size_t frames) {
  std::vector<float> out(frames * 2);
  engine.render(out);
  return out;
}
}  // namespace

TEST_CASE("a sound played on the main thread is heard at the next render") {
  AudioEngine engine;
  const AudioClipId clip = engine.clips().add("tone", flat(0.5F, 100));
  const SoundId sound = engine.play({.clip = clip});
  REQUIRE(sound.value == 1);
  REQUIRE(engine.play({.clip = clip}).value == 2);
  REQUIRE(render(engine, 4)[0] == Approx(1.0F));
  REQUIRE(engine.liveVoices() == 2);
  REQUIRE(engine.framesRendered() == 4);
  REQUIRE(engine.soundsPlayed() == 2);
}

TEST_CASE("a clip the bank does not have is refused with no id") {
  AudioEngine engine;
  REQUIRE(engine.play({.clip = AudioClipId{4}}) == SoundId{});
  REQUIRE(engine.soundsDropped() == 1);
  REQUIRE(engine.soundsPlayed() == 0);
}

TEST_CASE("stop, volume and the listener reach the mixer in order") {
  AudioEngine engine;
  const AudioClipId clip = engine.clips().add("tone", flat(0.5F, 48000));
  const SoundId sound = engine.play({.clip = clip});
  engine.setBusGain(AudioBus::EFFECTS, 0.5F);
  REQUIRE(render(engine, 1)[0] == Approx(0.25F));
  engine.setMasterGain(0.0F);
  REQUIRE(render(engine, 1)[0] == 0.0F);
  engine.stop(sound);
  (void)render(engine, 4800);
  REQUIRE(engine.liveVoices() == 0);
}

TEST_CASE("a queue full of requests refuses the rest until a render") {
  AudioEngine engine;
  const AudioClipId clip = engine.clips().add("tone", flat(0.5F, 10));
  for (uint32_t i = 0; i < AUDIO_COMMAND_CAPACITY; ++i) {
    REQUIRE(engine.play({.clip = clip}) != SoundId{});
  }
  REQUIRE(engine.play({.clip = clip}) == SoundId{});
  (void)render(engine, 1);
  REQUIRE(engine.play({.clip = clip}) != SoundId{});
}
