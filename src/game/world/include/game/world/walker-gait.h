#pragma once

/// @file walker-gait.h
/// @brief How far along its stride a walker the world follows is.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/math/vec3.h>

namespace eng::game {

/// How far, in tiles, a walker may move in one tick and still be walking.
/// Further is a spawn, a revive or the logic's `moveTo`, which is no step.
inline constexpr float WALKER_TELEPORT_TILES = 1.0F;

/// What the world keeps of one walker between ticks while game logic
/// listens for steps: simulation state, so it is hashed then.
struct WalkerGait {
  /// Where it was last tick.
  Vec3 last{};
  /// How far it has come since its last step, in tiles.
  float travelled = 0.0F;
  /// How far it walks between steps, in tiles: its feet's.
  float stride = 0.75F;
  /// 1 once it has been seen; a walker first seen starts half a stride in.
  uint8_t known = 0;
};

/// Move @p gait on to @p at, and say whether that completed a stride — one
/// step a tick at most, the rest carried on towards the next.
[[nodiscard]] bool walkOn(WalkerGait& gait, Vec3 at);

}  // namespace eng::game
