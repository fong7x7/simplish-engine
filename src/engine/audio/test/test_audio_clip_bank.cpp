#include <catch2/catch_test_macros.hpp>
#include <engine/audio/audio-clip-bank.h>

using eng::audio::AudioClip;
using eng::audio::AudioClipBank;
using eng::audio::AudioClipId;

TEST_CASE("clips are found by the name they were added under") {
  AudioClipBank bank;
  const AudioClipId shot = bank.add("shot", AudioClip{.samples = {0.1F}});
  const AudioClipId blast = bank.add("blast", AudioClip{.samples = {0.2F}});
  REQUIRE(bank.size() == 2);
  REQUIRE(bank.find("shot") == shot);
  REQUIRE(bank.find("blast") == blast);
  REQUIRE_FALSE(bank.find("bite").has_value());
  REQUIRE(bank.clip(blast)->samples[0] == 0.2F);
}

TEST_CASE("an id past the bank's end is no clip") {
  const AudioClipBank bank;
  REQUIRE(bank.clip(AudioClipId{3}) == nullptr);
}

TEST_CASE("adding a name again replaces its clip and keeps the old alive") {
  AudioClipBank bank;
  const AudioClipId first = bank.add("shot", AudioClip{.samples = {0.1F}});
  const AudioClip* old = bank.clip(first);
  const AudioClipId again = bank.add("shot", AudioClip{.samples = {0.9F}});
  REQUIRE(again == first);
  REQUIRE(bank.size() == 1);
  REQUIRE(bank.clip(again)->samples[0] == 0.9F);
  // A voice that was handed the old clip can still read it.
  REQUIRE(old->samples[0] == 0.1F);
}

TEST_CASE("a clip's address holds still as others are added") {
  AudioClipBank bank;
  const AudioClip* first = bank.clip(bank.add("a", AudioClip{.samples = {1}}));
  for (int i = 0; i < 100; ++i) {
    bank.add("clip" + std::to_string(i), AudioClip{.samples = {0.0F}});
  }
  REQUIRE(bank.clip(AudioClipId{0}) == first);
}
