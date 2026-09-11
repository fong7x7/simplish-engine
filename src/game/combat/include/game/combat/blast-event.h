#pragma once

/// @file blast-event.h
/// @brief An explosion, waiting to hurt everyone near it.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/math/vec2.h>
#include <game/combat/combatant-ref.h>

namespace eng::game {

/// A blast: everyone within `radius` of `at` — players and actors, on
/// every side — loses `damage`, except whoever went off (Game §5.1: a
/// bloater damages players and enemies alike).
struct BlastEvent {
  /// Where it goes off, on the floor.
  Vec2 at{};
  /// How far it reaches, in tiles.
  float radius = 0.0F;
  /// Segments everyone it reaches loses.
  uint16_t damage = 0;
  /// Whoever went off, whom it spares.
  CombatantRef source{};
};

}  // namespace eng::game
