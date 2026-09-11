#pragma once

/// @file combat-body.h
/// @brief Someone a shot, a pool or a blast can catch, as combat sees them.
/// @par Threading
/// A value type.

#include <engine/math/vec2.h>
#include <game/combat/combatant-ref.h>
#include <game/content/faction.h>

namespace eng::game {

/// A player or actor standing somewhere this tick: who, where, how wide,
/// and which side. The world lists everyone this way once a tick for the
/// combat phases, which never see the pools themselves.
struct CombatBody {
  /// Who it is.
  CombatantRef who{};
  /// Where they stand, on the floor.
  Vec2 at{};
  /// How wide they are, in tiles of radius.
  float radius = 0.0F;
  /// Their side. Players are friendly.
  Faction side = Faction::FRIENDLY;
};

}  // namespace eng::game
