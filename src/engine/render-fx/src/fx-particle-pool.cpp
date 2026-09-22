#include <algorithm>
#include <cmath>
#include <engine/render-fx/fx-particle-pool.h>
#include <numbers>
#include <utility>

namespace eng {

namespace {

  /// Shorter than this, a direction is taken to be no direction at all.
  constexpr float NO_DIRECTION = 1e-6f;

  /// Degrees in a turn, which is the range a particle's angle is drawn from.
  constexpr float FULL_TURN = 360.0f;

  /// A value between @p lo and @p hi, uniformly, from @p rng.
  float between(float lo, float hi, Pcg32& rng) {
    return lo + (hi - lo) * rng.nextUnitFloat();
  }

  /// Two unit vectors at right angles to unit @p w and to each other.
  std::pair<Vec3, Vec3> basisAround(const Vec3& w) {
    const Vec3 helper =
        std::abs(w.z) < 0.9f ? Vec3{0.0f, 0.0f, 1.0f} : Vec3{1.0f, 0.0f, 0.0f};
    const Vec3 u = Vec3::normalize(Vec3::cross(helper, w));
    return {u, Vec3::cross(w, u)};
  }

  /// A unit vector uniformly inside the cone of half-angle @p degrees about
  /// @p axis — every way when @p axis is no direction.
  Vec3 directionInCone(const Vec3& axis, float degrees, Pcg32& rng) {
    const float length = Vec3::length(axis);
    const Vec3 w = length > NO_DIRECTION ? axis / length : Vec3{0, 0, 1};
    const float spread = length > NO_DIRECTION ? degrees : 180.0f;
    const float cos_max = std::cos(spread * std::numbers::pi_v<float> / 180.0f);
    // Uniform over the sphere's cap: the cosine is uniform, not the angle.
    const float cos_t = 1.0f - rng.nextUnitFloat() * (1.0f - cos_max);
    const float sin_t = std::sqrt(std::max(0.0f, 1.0f - cos_t * cos_t));
    const float phi = 2.0f * std::numbers::pi_v<float> * rng.nextUnitFloat();
    const auto [u, v] = basisAround(w);
    return w * cos_t + u * (sin_t * std::cos(phi)) +
           v * (sin_t * std::sin(phi));
  }

  /// One burst being thrown out: what, from where, and the stream its
  /// particles' spread is drawn from.
  struct Emission {
    /// The burst.
    const FxBurst& burst;
    /// Where and which way.
    const FxEmit& emit;
    /// The `fx` stream.
    Pcg32& rng;
  };

  /// Put a new particle of @p e at index @p i.
  void place(FxParticlePool& pool, uint32_t i, const Emission& e) {
    const FxBurst& burst = e.burst;
    const Vec3 way =
        directionInCone(e.emit.direction, burst.spread_degrees, e.rng);
    pool.position[i] = e.emit.at;
    pool.velocity[i] =
        way * (between(burst.speed_min, burst.speed_max, e.rng) * e.emit.scale);
    pool.age[i] = 0.0f;
    pool.life[i] =
        std::max(between(burst.life_min, burst.life_max, e.rng), 1e-3f);
    pool.look[i] = burst.look;
    pool.scale[i] = e.emit.scale;
    // Its own angle, so a burst of puffs never turns as one, and its own
    // seed, so no two of them are broken up the same way.
    pool.angle[i] = e.rng.nextUnitFloat() * FULL_TURN;
  }

  /// Keep a particle that has reached the floor on it, bouncing a little.
  void meetFloor(Vec3& position, Vec3& velocity) {
    if (position.z >= 0.0f) {
      return;
    }
    position.z = 0.0f;
    velocity = {velocity.x * FX_FLOOR_SKID, velocity.y * FX_FLOOR_SKID,
                -velocity.z * FX_FLOOR_BOUNCE};
  }

  /// Move the particle at @p i on by @p seconds.
  void stepOne(FxParticlePool& pool, uint32_t i, float seconds) {
    const FxParticleLook& look = pool.look[i];
    Vec3 v = pool.velocity[i];
    v.z -= look.gravity * seconds;
    v = v * std::exp(-look.drag * seconds);
    Vec3 p = pool.position[i] + v * seconds;
    meetFloor(p, v);
    pool.position[i] = p;
    pool.velocity[i] = v;
    pool.age[i] += seconds;
  }

  /// Move the last live particle into @p i's place, and drop the last.
  void removeAt(FxParticlePool& pool, uint32_t i) {
    const uint32_t last = --pool.live;
    pool.position[i] = pool.position[last];
    pool.velocity[i] = pool.velocity[last];
    pool.age[i] = pool.age[last];
    pool.life[i] = pool.life[last];
    pool.look[i] = pool.look[last];
    pool.scale[i] = pool.scale[last];
    pool.angle[i] = pool.angle[last];
  }

}  // namespace

FxParticlePool::FxParticlePool(uint32_t capacity)
  : position(capacity), velocity(capacity), age(capacity), life(capacity),
    look(capacity), scale(capacity), angle(capacity) {}

uint32_t emitFxBurst(FxParticlePool& pool, const FxBurst& burst,
                     const FxEmit& emit, Pcg32& rng) {
  const auto capacity = static_cast<uint32_t>(pool.position.size());
  const uint32_t fit = std::min<uint32_t>(burst.count, capacity - pool.live);
  const Emission emission{burst, emit, rng};
  for (uint32_t k = 0; k < fit; ++k) {
    place(pool, pool.live++, emission);
  }
  return fit;
}

void stepFxParticles(FxParticlePool& pool, float seconds) {
  uint32_t i = 0;
  while (i < pool.live) {
    stepOne(pool, i, seconds);
    if (pool.age[i] >= pool.life[i]) {
      removeAt(pool, i);
    } else {
      ++i;
    }
  }
}

void clearFxParticles(FxParticlePool& pool) {
  pool.live = 0;
}

float fxParticleProgress(const FxParticlePool& pool, uint32_t index) {
  return std::clamp(pool.age[index] / pool.life[index], 0.0f, 1.0f);
}

}  // namespace eng
