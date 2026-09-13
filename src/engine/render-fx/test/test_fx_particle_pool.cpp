#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <engine/render-fx/fx-particle-pool.h>
#include <numbers>

using Catch::Approx;
using namespace eng;

namespace {

/// A burst of @p count particles thrown between 2 and 4 tiles a second, in
/// a cone of @p spread degrees, living between half a second and a second.
FxBurst testBurst(uint16_t count, float spread) {
  FxBurst burst;
  burst.count = count;
  burst.spread_degrees = spread;
  burst.speed_min = 2.0f;
  burst.speed_max = 4.0f;
  burst.life_min = 0.5f;
  burst.life_max = 1.0f;
  return burst;
}

/// One particle in @p pool, at @p at, moving at @p velocity, with @p look,
/// living a second.
void placeOne(FxParticlePool& pool, Vec3 at, Vec3 velocity,
              const FxParticleLook& look) {
  Pcg32 rng(1, 2);
  FxBurst burst = testBurst(1, 0.0f);
  burst.look = look;
  burst.life_min = 1.0f;
  burst.life_max = 1.0f;
  (void)emitFxBurst(pool, burst, {at, {1, 0, 0}, 1.0f}, rng);
  pool.velocity[0] = velocity;
}

}  // namespace

TEST_CASE("a burst puts every particle it has room for at its emit point",
          "[render-fx][particles]") {
  FxParticlePool pool(10);
  Pcg32 rng(7, 1);
  const FxEmit emit{{1.0f, 2.0f, 0.5f}, {1.0f, 0.0f, 0.0f}, 1.0f};

  CHECK(emitFxBurst(pool, testBurst(6, 30.0f), emit, rng) == 6);
  CHECK(pool.live == 6);
  // Only four more fit.
  CHECK(emitFxBurst(pool, testBurst(6, 30.0f), emit, rng) == 4);
  CHECK(pool.live == 10);
  for (uint32_t i = 0; i < pool.live; ++i) {
    CHECK(pool.position[i].x == 1.0f);
    CHECK(pool.position[i].z == 0.5f);
    CHECK(pool.age[i] == 0.0f);
    CHECK(pool.life[i] >= 0.5f);
    CHECK(pool.life[i] <= 1.0f);
  }
}

TEST_CASE("a burst's particles leave inside its cone, at its speeds, scaled",
          "[render-fx][particles]") {
  FxParticlePool pool(64);
  Pcg32 rng(3, 9);
  const Vec3 axis{0.0f, 1.0f, 0.0f};
  const float spread = 25.0f;
  (void)emitFxBurst(pool, testBurst(64, spread), {{}, axis * 5.0f, 2.0f}, rng);

  const float cos_max = std::cos(spread * std::numbers::pi_v<float> / 180.0f);
  for (uint32_t i = 0; i < pool.live; ++i) {
    const float speed = Vec3::length(pool.velocity[i]);
    // The emit's scale doubles every speed.
    CHECK(speed >= 4.0f - 1e-4f);
    CHECK(speed <= 8.0f + 1e-4f);
    CHECK(Vec3::dot(pool.velocity[i] / speed, axis) >= cos_max - 1e-4f);
    CHECK(pool.scale[i] == 2.0f);
  }
}

TEST_CASE("a burst with no direction throws its particles every way",
          "[render-fx][particles]") {
  FxParticlePool pool(256);
  Pcg32 rng(11, 4);
  (void)emitFxBurst(pool, testBurst(256, 10.0f), {{}, {}, 1.0f}, rng);

  bool up = false;
  bool down = false;
  for (uint32_t i = 0; i < pool.live; ++i) {
    up = up || pool.velocity[i].z > 0.5f;
    down = down || pool.velocity[i].z < -0.5f;
  }
  CHECK(up);
  CHECK(down);
}

TEST_CASE("the same seed throws the same burst", "[render-fx][particles]") {
  FxParticlePool a(8);
  FxParticlePool b(8);
  Pcg32 rng_a(5, 5);
  Pcg32 rng_b(5, 5);
  const FxEmit emit{{}, {1, 0, 0}, 1.0f};
  (void)emitFxBurst(a, testBurst(8, 40.0f), emit, rng_a);
  (void)emitFxBurst(b, testBurst(8, 40.0f), emit, rng_b);

  for (uint32_t i = 0; i < 8; ++i) {
    CHECK(a.velocity[i].x == b.velocity[i].x);
    CHECK(a.velocity[i].y == b.velocity[i].y);
    CHECK(a.life[i] == b.life[i]);
  }
}

TEST_CASE("an unhindered particle moves in a straight line",
          "[render-fx][particles]") {
  FxParticlePool pool(1);
  placeOne(pool, {0, 0, 1}, {2, 0, 0}, {});
  stepFxParticles(pool, 0.25f);
  CHECK(pool.position[0].x == Approx(0.5f));
  CHECK(pool.position[0].z == Approx(1.0f));
  CHECK(pool.age[0] == Approx(0.25f));
}

TEST_CASE("gravity pulls a particle down, and negative gravity lifts it",
          "[render-fx][particles]") {
  FxParticleLook falls;
  falls.gravity = 4.0f;
  FxParticlePool pool(1);
  placeOne(pool, {0, 0, 5}, {}, falls);
  stepFxParticles(pool, 0.5f);
  CHECK(pool.velocity[0].z == Approx(-2.0f));

  FxParticleLook rises;
  rises.gravity = -4.0f;
  FxParticlePool smoke(1);
  placeOne(smoke, {0, 0, 5}, {}, rises);
  stepFxParticles(smoke, 0.5f);
  CHECK(smoke.velocity[0].z == Approx(2.0f));
}

TEST_CASE("drag slows a particle by a factor of e every 1 / drag seconds",
          "[render-fx][particles]") {
  FxParticleLook slows;
  slows.drag = 2.0f;
  FxParticlePool pool(1);
  placeOne(pool, {0, 0, 1}, {3, 0, 0}, slows);
  stepFxParticles(pool, 0.5f);
  CHECK(pool.velocity[0].x == Approx(3.0f / std::numbers::e_v<float>));
}

TEST_CASE("a particle reaching the floor stays on it, bouncing a little",
          "[render-fx][particles]") {
  FxParticlePool pool(1);
  placeOne(pool, {0, 0, 0.1f}, {1, 0, -2}, {});
  stepFxParticles(pool, 0.1f);

  CHECK(pool.position[0].z == 0.0f);
  CHECK(pool.velocity[0].z == Approx(2.0f * FX_FLOOR_BOUNCE));
  CHECK(pool.velocity[0].x == Approx(FX_FLOOR_SKID));
}

TEST_CASE("a particle that has lived its life is dropped, and the rest kept",
          "[render-fx][particles]") {
  FxParticlePool pool(3);
  Pcg32 rng(2, 2);
  FxBurst burst = testBurst(3, 0.0f);
  (void)emitFxBurst(pool, burst, {{}, {1, 0, 0}, 1.0f}, rng);
  pool.life[0] = 0.1f;
  pool.life[1] = 2.0f;
  pool.life[2] = 0.2f;

  stepFxParticles(pool, 0.15f);
  REQUIRE(pool.live == 2);
  stepFxParticles(pool, 0.1f);
  REQUIRE(pool.live == 1);
  CHECK(pool.life[0] == 2.0f);
  CHECK(fxParticleProgress(pool, 0) == Approx(0.125f));

  clearFxParticles(pool);
  CHECK(pool.live == 0);
}
