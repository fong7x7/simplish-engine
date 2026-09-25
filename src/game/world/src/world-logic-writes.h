#pragma once

/// @file world-logic-writes.h
/// @brief Where game logic's writes wait for the world to apply them.
/// @par Threading
/// A view over a world's own lists, for one call of the logic.

#include <game/actors/actor-spawn.h>
#include <game/combat/combat-effects.h>
#include <game/world/logic-command.h>
#include <vector>

namespace eng::game {

/// What a logic's call asks of the world, queued for it to apply in order
/// when the logic returns.
struct WorldLogicWrites {
  /// Where the logic's writes to players and actors are queued.
  std::vector<LogicCommand>& commands;
  /// Where the logic's spawns are queued.
  std::vector<ActorSpawn>& spawns;
  /// Where the logic's shots, blasts and hazard pools are queued.
  CombatEffects& combat;
};

}  // namespace eng::game
