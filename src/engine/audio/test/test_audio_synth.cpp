#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <engine/audio/audio-synth.h>

using Catch::Approx;
using eng::audio::clipSeconds;
using eng::audio::synthesize;
using eng::audio::SynthSpec;

namespace {
float peak(const std::vector<float>& samples) {
  float loudest = 0.0F;
  for (const float s : samples) {
    loudest = std::max(loudest, std::abs(s));
  }
  return loudest;
}
}  // namespace

TEST_CASE("a synthesised clip lasts as long as its spec, in mono") {
  const auto clip = synthesize({.seconds = 0.25F}, 48000);
  REQUIRE(clip.channels == 1);
  REQUIRE(clip.sample_rate == 48000);
  REQUIRE(clipSeconds(clip) == Approx(0.25F));
}

TEST_CASE("a synthesised clip peaks just under full scale and ends silent") {
  const auto clip = synthesize({.seconds = 0.3F, .decay_seconds = 1.0F}, 48000);
  REQUIRE(peak(clip.samples) == Approx(0.9F));
  REQUIRE(std::abs(clip.samples.back()) < 0.01F);
}

TEST_CASE("the same spec makes the same samples, and another seed others") {
  const SynthSpec spec{.seconds = 0.1F, .seed = 7};
  SynthSpec other = spec;
  other.seed = 8;
  REQUIRE(synthesize(spec, 44100).samples == synthesize(spec, 44100).samples);
  REQUIRE(synthesize(spec, 44100).samples != synthesize(other, 44100).samples);
}

TEST_CASE("a pure tone crosses zero at its pitch") {
  const auto clip = synthesize({.seconds = 1.0F,
                                .decay_seconds = 100.0F,
                                .tone_start_hz = 100.0F,
                                .tone_end_hz = 100.0F,
                                .tone_level = 1.0F,
                                .noise_level = 0.0F},
                               48000);
  int crossings = 0;
  for (size_t i = 1; i < clip.samples.size(); ++i) {
    crossings += (clip.samples[i - 1] < 0.0F) != (clip.samples[i] < 0.0F);
  }
  // Two a cycle; the fade at either end may cost one.
  REQUIRE(crossings >= 198);
  REQUIRE(crossings <= 201);
}

TEST_CASE("a silent spec is silence, not a division by zero") {
  const auto clip = synthesize({.tone_level = 0.0F, .noise_level = 0.0F}, 8000);
  REQUIRE(peak(clip.samples) == 0.0F);
}
