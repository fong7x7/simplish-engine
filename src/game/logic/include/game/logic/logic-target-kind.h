#pragma once

/// @file logic-target-kind.h
/// @brief Which pool something game logic names is in.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// Which pool a `LogicTarget` names an entity of. The pools number their
/// entities independently, so a target is a pool and a handle together.
enum class LogicTargetKind : uint8_t {
  /// A player, in the player pool.
  PLAYER,
  /// An actor — an enemy or an NPC — in the actor pool.
  ACTOR,
};

}  // namespace eng::game
