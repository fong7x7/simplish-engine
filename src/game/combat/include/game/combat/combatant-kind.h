#pragma once

/// @file combatant-kind.h
/// @brief Which pool something that can be hurt is in.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// Which pool a handle to something that can be hurt — or targeted —
/// names an entity of: the pools number their handles independently.
enum class CombatantKind : uint8_t {
  /// A player, in the player pool.
  PLAYER,
  /// An actor, in the actor pool.
  ACTOR,
};

}  // namespace eng::game
