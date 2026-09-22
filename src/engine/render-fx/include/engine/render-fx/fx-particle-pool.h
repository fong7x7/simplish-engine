#pragma once

/// @file fx-particle-pool.h
/// @brief Every live particle, as structure-of-arrays, and what moves them.
/// @par Threading
/// Main-thread-only.
///
/// Presentation, never simulation (ADR-002): particles are moved by the
/// render frame's time, spread by a cosmetic random stream, and nothing in
/// a tick reads them — so they may use libm and never reach a tick hash.

#include <cstdint>
#include <engine/core/pcg32.h>
#include <engine/math/vec3.h>
#include <engine/render-fx/fx-burst.h>
#include <engine/render-fx/fx-emit.h>
#include <engine/render-fx/fx-particle-look.h>
#include <vector>

namespace eng {

/// Particles a pool holds at once unless it is given another number.
inline constexpr uint32_t FX_PARTICLE_CAPACITY = 4096;

/// What a particle's vertical speed keeps, turned upward, when it reaches
/// the floor: it bounces a little, rather than sinking through it.
inline constexpr float FX_FLOOR_BOUNCE = 0.3f;

/// What a particle's speed along the floor keeps when it reaches it.
inline constexpr float FX_FLOOR_SKID = 0.6f;

/// Every particle alive, packed: the first `live` entries of each array
/// are the live ones, in no particular order — a dead one's place is taken
/// by the last. Sized once, so emitting never allocates; a burst past the
/// capacity loses the particles that do not fit.
struct FxParticlePool {
  /// A pool with room for @p capacity particles.
  explicit FxParticlePool(uint32_t capacity = FX_PARTICLE_CAPACITY);

  /// How many particles are alive.
  uint32_t live = 0;
  /// Where each is, in tiles; z is its height above the floor.
  std::vector<Vec3> position;
  /// How fast each moves, and which way, in tiles per second.
  std::vector<Vec3> velocity;
  /// Seconds each has lived.
  std::vector<float> age;
  /// Seconds each lives in all.
  std::vector<float> life;
  /// How each looks, from its burst.
  std::vector<FxParticleLook> look;
  /// How big each is drawn, times its look's sizes: its emit's scale.
  std::vector<float> scale;
  /// The angle each was thrown at, in degrees, and the seed its puff's
  /// noise is broken up by: one number, since both only have to differ
  /// between particles. Drawn when the particle is emitted.
  std::vector<float> angle;
};

/// Throw @p burst out of @p emit into @p pool, each particle's direction,
/// speed and life drawn from @p rng. How many fitted.
uint32_t emitFxBurst(FxParticlePool& pool, const FxBurst& burst,
                     const FxEmit& emit, Pcg32& rng);

/// Move every particle on by @p seconds — falling, slowing, bouncing off
/// the floor at z = 0 — and drop those that have lived their life.
void stepFxParticles(FxParticlePool& pool, float seconds);

/// Drop every particle.
void clearFxParticles(FxParticlePool& pool);

/// How far through its life the live particle at @p index is, 0 to 1.
[[nodiscard]] float fxParticleProgress(const FxParticlePool& pool,
                                       uint32_t index);

}  // namespace eng
