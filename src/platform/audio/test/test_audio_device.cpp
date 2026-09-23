#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdlib>
#include <engine/audio/audio-device.h>
#include <thread>

using eng::audio::AudioClip;
using eng::audio::AudioDevice;
using eng::audio::AudioEngine;

namespace {

/// Wait up to two seconds for @p done.
template <typename Done> bool waitFor(Done done) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!done() && std::chrono::steady_clock::now() < until) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return done();
}

/// SDL's dummy driver pulls samples in real time with no hardware, so the
/// tests hold on a machine with no speakers — a CI runner.
void useDummyDriver() {
  // Set before SDL's audio starts, while this test's thread is the only
  // one: nothing reads the environment concurrently.
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  setenv("SDL_AUDIO_DRIVER", "dummy", 1);
}

}  // namespace

#if ENGINE_AUDIO_BACKEND_NONE

TEST_CASE("no backend says so, and is never open") {
  AudioEngine engine;
  AudioDevice device;
  REQUIRE(device.open(engine).has_value());
  REQUIRE_FALSE(device.isOpen());
  device.close();
}

#else

TEST_CASE("the device opens, closes, and does both twice without harm") {
  useDummyDriver();
  AudioEngine engine;
  AudioDevice device;
  REQUIRE_FALSE(device.open(engine).has_value());
  REQUIRE_FALSE(device.open(engine).has_value());
  REQUIRE(device.isOpen());
  device.close();
  device.close();
  REQUIRE_FALSE(device.isOpen());
}

TEST_CASE("an open device pulls the mix, and a sound plays out") {
  useDummyDriver();
  AudioEngine engine;
  const auto clip = engine.clips().add(
      "blip", AudioClip{.samples = std::vector<float>(2400, 0.5F)});
  AudioDevice device;
  REQUIRE_FALSE(device.open(engine).has_value());
  REQUIRE(engine.play({.clip = clip}).value != 0);
  REQUIRE(waitFor([&] { return engine.framesRendered() > 2400; }));
  REQUIRE(waitFor([&] { return engine.liveVoices() == 0; }));
  device.close();
  const uint64_t frames = engine.framesRendered();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  REQUIRE(engine.framesRendered() == frames);
}

#endif
