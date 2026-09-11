#pragma once

/// @file actor-intent.h
/// @brief What one actor means to do this tick: worked out, used, and
/// thrown away within it.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/math/vec2.h>

namespace eng::game {

/// One actor's working for one tick. Not state: every field is set afresh
/// by the tick's passes before it is read, so none of it is hashed.
struct ActorIntent {
  /// Whether its action wants it to move toward its goal.
  uint8_t moves = 0;
  /// How close to its goal counts as there, in tiles.
  float stop_within = 0.0F;
  /// Whether its goal is the player it perceives this tick — the goal that
  /// player's flow field leads to.
  uint8_t chases = 0;
  /// The displacement steering and separation want this tick, in tiles.
  Vec2 step{};
  /// The displacement it actually made, after collision.
  Vec2 moved{};
};

}  // namespace eng::game
