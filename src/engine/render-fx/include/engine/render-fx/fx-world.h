#pragma once

/// @file fx-world.h
/// @brief Every effect playing: its particles, its flashes, and the random
/// stream that spreads them.
/// @par Threading
/// Main-thread-only.

#include <cstdint>
#include <engine/core/pcg32.h>
#include <engine/render-fx/fx-effect.h>
#include <engine/render-fx/fx-emit.h>
#include <engine/render-fx/fx-light-pool.h>
#include <engine/render-fx/fx-particle-pool.h>

namespace eng {

/// The PCG32 stream effects draw from — Engine REQUIREMENTS §4.3's `fx`
/// stream. Cosmetic: it lives outside the simulation and no tick hash ever
/// sees it, so however it is drawn from, no session can desync over it.
inline constexpr uint64_t FX_RNG_STREAM = 0x6678;

/// Everything the effects pass draws and lights, stepped on the render
/// frame's clock. Whatever plays effects — the playtest, and the game's
/// client when there is one — holds one of these, feeds it effects as the
/// simulation reports what happened, and steps it each frame.
struct FxWorld {
  /// An empty world whose stream is seeded from @p seed.
  explicit FxWorld(uint64_t seed);

  /// Every particle alive.
  FxParticlePool particles;
  /// Every flash shining.
  FxLightPool lights;
  /// The `fx` stream every burst's spread is drawn from.
  Pcg32 rng;
};

/// Play @p effect at @p emit: every burst thrown out, and the flash lit.
void playFxEffect(FxWorld& world, const FxEffect& effect, const FxEmit& emit);

/// Move every particle and age every flash by @p seconds.
void stepFxWorld(FxWorld& world, float seconds);

/// Stop every effect at once.
void clearFxWorld(FxWorld& world);

}  // namespace eng
