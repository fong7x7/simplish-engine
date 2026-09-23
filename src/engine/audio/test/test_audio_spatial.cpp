#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/audio/audio-spatial.h>

using Catch::Approx;
using eng::Vec3;
using eng::audio::AUDIO_MAX_PAN;
using eng::audio::AudioListener;
using eng::audio::distanceGain;
using eng::audio::pan;
using eng::audio::panGain;
using eng::audio::spatialGain;
using eng::audio::StereoGain;

namespace {
const AudioListener LISTENER{
    .at = {10.0F, 10.0F, 0.0F}, .full_tiles = 2.0F, .silent_tiles = 12.0F};
}

TEST_CASE("a sound is full near the listener and silent far off") {
  REQUIRE(distanceGain(LISTENER, {11.0F, 10.0F, 0.0F}) == 1.0F);
  REQUIRE(distanceGain(LISTENER, {10.0F, 30.0F, 0.0F}) == 0.0F);
  const float mid = distanceGain(LISTENER, {17.0F, 10.0F, 0.0F});
  REQUIRE(mid == Approx(0.25F));
}

TEST_CASE("height does not make a sound further away") {
  REQUIRE(distanceGain(LISTENER, {10.0F, 10.0F, 50.0F}) == 1.0F);
}

TEST_CASE("a sound pans toward the side of the screen it is on") {
  REQUIRE(pan(LISTENER, {10.0F, 10.0F, 0.0F}) == 0.0F);
  REQUIRE(pan(LISTENER, {12.0F, 10.0F, 0.0F}) > 0.0F);
  REQUIRE(pan(LISTENER, {8.0F, 10.0F, 0.0F}) < 0.0F);
  REQUIRE(pan(LISTENER, {100.0F, 10.0F, 0.0F}) == Approx(AUDIO_MAX_PAN));
  // Straight down the screen is neither side.
  REQUIRE(pan(LISTENER, {10.0F, 18.0F, 0.0F}) == 0.0F);
}

TEST_CASE("screen-right follows the camera, not the world's x") {
  AudioListener turned = LISTENER;
  turned.right = {0.0F, 1.0F};
  REQUIRE(pan(turned, {10.0F, 13.0F, 0.0F}) > 0.0F);
  REQUIRE(pan(turned, {13.0F, 10.0F, 0.0F}) == 0.0F);
}

TEST_CASE("the centre of the pan is unity in both ears") {
  const StereoGain centre = panGain(0.0F);
  REQUIRE(centre.left == Approx(1.0F));
  REQUIRE(centre.right == Approx(1.0F));
  const StereoGain right = panGain(0.7F);
  REQUIRE(right.right > right.left);
  REQUIRE(right.left > 0.0F);
  // Equal power: the energy in the two ears adds to the centre's.
  REQUIRE(right.left * right.left + right.right * right.right == Approx(2.0F));
}

TEST_CASE("a sound out of earshot is silent in both ears") {
  const StereoGain gain = spatialGain(LISTENER, {40.0F, 10.0F, 0.0F});
  REQUIRE(gain.left == 0.0F);
  REQUIRE(gain.right == 0.0F);
}
