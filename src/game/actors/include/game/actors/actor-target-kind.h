#pragma once

/// @file actor-target-kind.h
/// @brief Which pool an actor's target is in.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// Which pool the handle in `ActorPool::target` names an entity of: the
/// two pools number their handles independently.
enum class ActorTargetKind : uint8_t {
  /// A player, in the player pool.
  PLAYER,
  /// Another actor, in the actor pool.
  ACTOR,
};

}  // namespace eng::game
