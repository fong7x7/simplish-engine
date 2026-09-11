#pragma once

/// @file actor-ref.h
/// @brief One actor, named by its pool and dense index.
/// @par Threading
/// A view; valid within one pass of a tick.

#include <cstdint>
#include <game/actors/actor-pool.h>

namespace eng::game {

/// The actor at dense index `i` of `pool` — what every per-actor function
/// in the passes takes, so each reads `a.pool.position[a.i]` rather than
/// threading the pool and the index through every call separately.
struct ActorRef {
  /// The pool the actor is in.
  ActorPool& pool;
  /// Its dense index.
  uint32_t i = 0;
};

}  // namespace eng::game
