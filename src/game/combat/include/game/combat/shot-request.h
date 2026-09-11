#pragma once

/// @file shot-request.h
/// @brief A projectile someone fired, waiting to be spawned.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/math/vec2.h>
#include <game/content/faction.h>

namespace eng::game {

/// One projectile to spawn in the weapon-fire phase: where from, how fast
/// which way, what it does, and whose side it is on — it hurts only the
/// other side.
struct ShotRequest {
  /// Where it starts, on the floor.
  Vec2 from{};
  /// How far it moves a tick, and which way, in tiles.
  Vec2 velocity{};
  /// Segments whoever it hits loses.
  uint16_t damage = 0;
  /// The side that fired it.
  Faction side = Faction::HOSTILE;
};

}  // namespace eng::game
