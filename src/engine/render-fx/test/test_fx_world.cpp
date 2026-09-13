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

/// Those bursts, and a flash lasting half a second, reaching two tiles.
constexpr FxEffect TEST_EFFECT{TEST_BURSTS, {{1, 1, 1}, 2.0f, 2.0f, 0.5f}};

}  // namespace

TEST_CASE("playing an effect throws out every burst and lights the flash",
          "[render-fx][world]") {
  FxWorld world(1);
  playFxEffect(world, TEST_EFFECT, {{1, 1, 1}, {1, 0, 0}, 3.0f});

  CHECK(world.particles.live == 8);
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
