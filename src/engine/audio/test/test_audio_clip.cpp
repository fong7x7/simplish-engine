#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/audio/audio-clip.h>

using eng::audio::AudioClip;
using eng::audio::clipFrames;
using eng::audio::clipSeconds;

TEST_CASE("a clip's frames count one sample a channel") {
  const AudioClip mono{.samples = std::vector<float>(480), .channels = 1};
  const AudioClip stereo{.samples = std::vector<float>(480), .channels = 2};
  REQUIRE(clipFrames(mono) == 480);
  REQUIRE(clipFrames(stereo) == 240);
}

TEST_CASE("a clip lasts its frames over its rate") {
  const AudioClip clip{.samples = std::vector<float>(22050),
                       .sample_rate = 44100,
                       .channels = 1};
  REQUIRE(clipSeconds(clip) == Catch::Approx(0.5F));
}

TEST_CASE("a clip with no channels or no rate is empty, not a crash") {
  REQUIRE(clipFrames(AudioClip{.samples = {1.0F}, .channels = 0}) == 0);
  REQUIRE(clipSeconds(AudioClip{.samples = {1.0F}, .sample_rate = 0}) == 0.0F);
}
