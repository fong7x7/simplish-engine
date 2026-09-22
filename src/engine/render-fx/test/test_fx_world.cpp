#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/render-fx/fx-world.h>

using namespace eng;

namespace {

/// Two bursts of three and five particles, a quarter of a second each.
constexpr std::array<FxBurst, 2> TEST_BURSTS{{
    {3, 180.0f, 1.0f, 2.0f, 0.25f, 0.25f, {}},
    {5, 30.0f, 1.0f, 2.0f, 0.25f, 0.25f, {}},
}};

/// One cloud of smoke a tile across, lasting two seconds.
constexpr std::array<FxVolume, 1> TEST_VOLUMES{{
    {{0.1f, 0.1f, 0.1f, 1.0f}, 2.0f, 0.5f, 0.5f, 0.0f, 0.0f, 2.0f},
}};

/// Those bursts and that cloud, and a flash lasting half a second,
/// reaching two tiles.
constexpr FxEffect TEST_EFFECT{
    TEST_BURSTS, TEST_VOLUMES, {{1, 1, 1}, 2.0f, 2.0f, 0.5f}};

}  // namespace

TEST_CASE("playing an effect throws out every burst and lights the flash",
          "[render-fx][world]") {
  FxWorld world(1);
  playFxEffect(world, TEST_EFFECT, {{1, 1, 1}, {1, 0, 0}, 3.0f});

  CHECK(world.particles.live == 8);
  CHECK(world.volumes.live == 1);
  REQUIRE(world.lights.live == 1);
  // The emit's scale reaches the flash's range as well as the particles.
  CHECK(world.lights.flash[0].range == 6.0f);
}

TEST_CASE("stepping the world ages particles and flashes, and clearing "
          "stops them",
          "[render-fx][world]") {
  FxWorld world(1);
  playFxEffect(world, TEST_EFFECT, {{}, {1, 0, 0}, 1.0f});

  stepFxWorld(world, 0.3f);
  CHECK(world.particles.live == 0);
  CHECK(world.lights.live == 1);
  stepFxWorld(world, 0.3f);
  CHECK(world.lights.live == 0);

  playFxEffect(world, TEST_EFFECT, {{}, {1, 0, 0}, 1.0f});
  clearFxWorld(world);
  CHECK(world.particles.live == 0);
  CHECK(world.lights.live == 0);
}
