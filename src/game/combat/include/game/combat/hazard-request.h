#pragma once

/// @file hazard-request.h
/// @brief A hazard pool someone lobbed, waiting to be spawned.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/math/vec2.h>
#include <game/content/faction.h>

namespace eng::game {

/// A pool of something nasty on the floor (Game §5.1's spitter): it hurts
/// the other side's players and actors standing in it, every so often,
/// until it dries up.
struct HazardRequest {
  /// Its centre, on the floor.
  Vec2 at{};
  /// Its radius, in tiles.
  float radius = 0.0F;
  /// Segments it takes from each it catches, each time it bites.
  uint16_t damage = 0;
  /// How long it lasts, in ticks.
  uint32_t ticks = 0;
  /// The side that lobbed it.
  Faction side = Faction::HOSTILE;
};

}  // namespace eng::game
