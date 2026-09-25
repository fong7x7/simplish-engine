#pragma once

/// @file logic-hazard.h
/// @brief A pool on the floor game logic leaves, biting who stands in it.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/math/vec3.h>
#include <game/content/faction.h>

namespace eng::game {

/// One hazard pool, for `GameLogicWorld::spawnHazard`: fire, acid, a trap —
/// it bites the bodies of the other side standing in it, as an actor's
/// spit does, until it dries up.
struct LogicHazard {
  /// Its centre, in tiles.
  Vec3 at{};
  /// How far it reaches, in tiles.
  float radius = 1.5F;
  /// Health segments each bite takes.
  uint16_t damage = 1;
  /// How long it lasts, in ticks.
  uint32_t ticks = 300;
  /// Whose it is: it bites only the other side.
  Faction side = Faction::FRIENDLY;
};

}  // namespace eng::game
