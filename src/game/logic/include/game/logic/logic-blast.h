#pragma once

/// @file logic-blast.h
/// @brief An explosion game logic sets off.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/math/vec3.h>
#include <game/logic/logic-target.h>
#include <optional>

namespace eng::game {

/// One blast, for `GameLogicWorld::blast`: it hurts every player and actor
/// within its radius, whatever their side, as an actor's death blast does
/// — and an actor it kills can go off in a blast of its own.
struct LogicBlast {
  /// Its centre, in tiles.
  Vec3 at{};
  /// How far it reaches, across the floor, in tiles.
  float radius = 2.0F;
  /// Health segments it takes from everyone it reaches.
  uint16_t damage = 1;
  /// Who set it off: credited with its hits. It spares nobody, them
  /// included. Empty for nobody.
  std::optional<LogicTarget> by{};
};

}  // namespace eng::game
