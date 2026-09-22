#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/render-fx/fx-volume-pool.h>
#include <engine/render-fx/fx-world.h>

using Catch::Approx;
using namespace eng;

namespace {

/// A cloud two tiles across and one tall, swelling and rising, lasting
/// four seconds.
FxVolume testVolume() {
  FxVolume volume;
  volume.color = {0.05f, 0.05f, 0.06f, 0.9f};
  volume.density = 2.0f;
  volume.radius = 1.0f;
  volume.height = 0.5f;
  volume.growth = 0.25f;
  volume.rise = 0.5f;
  volume.life = 4.0f;
  return volume;
}

}  // namespace

TEST_CASE("a cloud starts where it is emitted, with a seed of its own",
          "[render-fx][volume]") {
  FxVolumePool pool(4);
  Pcg32 rng(1, FX_RNG_STREAM);
  REQUIRE(
      emitFxVolume(pool, testVolume(), {{2.0f, 3.0f, 0.5f}, {}, 1.0f}, rng));
  REQUIRE(
      emitFxVolume(pool, testVolume(), {{2.0f, 3.0f, 0.5f}, {}, 1.0f}, rng));

  REQUIRE(pool.live == 2);
  REQUIRE(pool.position[0].x == 2.0f);
  REQUIRE(pool.age[0] == 0.0f);
  REQUIRE(pool.seed[0] != pool.seed[1]);
}

TEST_CASE("a full pool drops a new cloud rather than stealing one",
          "[render-fx][volume]") {
  FxVolumePool pool(1);
  Pcg32 rng(1, FX_RNG_STREAM);
  REQUIRE(emitFxVolume(pool, testVolume(), {}, rng));
  REQUIRE_FALSE(emitFxVolume(pool, testVolume(), {}, rng));
  REQUIRE(pool.live == 1);
}

TEST_CASE("a cloud rises, swells, and is gone at the end of its life",
          "[render-fx][volume]") {
  FxVolumePool pool(4);
  Pcg32 rng(1, FX_RNG_STREAM);
  REQUIRE(
      emitFxVolume(pool, testVolume(), {{0.0f, 0.0f, 1.0f}, {}, 2.0f}, rng));

  stepFxVolumes(pool, 2.0f);
  REQUIRE(pool.position[0].z == Approx(1.0f + 0.5f * 2.0f * 2.0f));
  REQUIRE(fxVolumeProgress(pool, 0) == Approx(0.5f));
  // Half a tile of growth, and the emit's scale on top of it.
  REQUIRE(fxVolumeExtents(pool, 0).x == Approx((1.0f + 0.5f) * 2.0f));
  REQUIRE(fxVolumeExtents(pool, 0).z == Approx((0.5f + 0.5f) * 2.0f));

  stepFxVolumes(pool, 2.0f);
  REQUIRE(pool.live == 0);
}

TEST_CASE("a cloud swells out of nothing and thins away again",
          "[render-fx][volume]") {
  FxVolumePool pool(4);
  Pcg32 rng(1, FX_RNG_STREAM);
  REQUIRE(emitFxVolume(pool, testVolume(), {}, rng));
  REQUIRE(fxVolumeDensity(pool, 0) == Approx(0.0f));

  // Fully in once the fade-in is past, then away with what life is left.
  stepFxVolumes(pool, 4.0f * FX_VOLUME_FADE_IN);
  REQUIRE(fxVolumeDensity(pool, 0) ==
          Approx(2.0f * (1.0f - FX_VOLUME_FADE_IN)));
  stepFxVolumes(pool, 4.0f * (0.75f - FX_VOLUME_FADE_IN));
  REQUIRE(fxVolumeDensity(pool, 0) == Approx(0.5f));
}

TEST_CASE("clearing a pool stops every cloud at once", "[render-fx][volume]") {
  FxVolumePool pool(4);
  Pcg32 rng(1, FX_RNG_STREAM);
  REQUIRE(emitFxVolume(pool, testVolume(), {}, rng));
  clearFxVolumes(pool);
  REQUIRE(pool.live == 0);
}
