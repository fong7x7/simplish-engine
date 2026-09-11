#pragma once

/// @file actor-candidate.h
/// @brief Someone an actor might take as its target, and where they rank.
/// @par Threading
/// A value type.

#include <cstdint>
#include <game/combat/combatant-kind.h>

namespace eng::game {

/// A player or actor within reach of an actor's senses, as perception ranks
/// them: the target it already has first, then the nearer, then players
/// before actors, then the lower dense index — a total order, so every
/// peer ranks the same list the same way.
struct ActorCandidate {
  /// Which pool they are in.
  CombatantKind kind = CombatantKind::PLAYER;
  /// Their dense index in that pool.
  uint32_t index = 0;
  /// Whether they are the actor's target already.
  uint8_t current = 0;
  /// How far away they are, squared.
  float distance_squared = 0.0F;
};

}  // namespace eng::game
