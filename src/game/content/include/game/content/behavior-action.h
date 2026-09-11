#pragma once

/// @file behavior-action.h
/// @brief What an actor does while in one state of its behavior.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// The closed set of things a behavior state can have an actor do
/// (ADR-009). Each is a case in the actor system, not a script: adding one
/// is adding code and a test, deliberately, so what content can make an
/// actor do stays enumerable.
///
/// "The target" is the player the actor last perceived, and "where it was
/// seen" is where the actor last perceived them — an actor never knows
/// where a player is without seeing or hearing them.
enum class BehaviorAction : uint8_t {
  /// Stand still. Turns to watch a target it can see.
  IDLE,
  /// Stand still, facing as the state says: a windup, a recovery, a pause.
  HOLD,
  /// Walk to random spots within `far_tiles` of home, pausing at each.
  WANDER,
  /// Walk to where the target was seen, stopping `near_tiles` short.
  PURSUE,
  /// Stay between `near_tiles` and `far_tiles` of the target: back off
  /// when it is closer, close in when it is further.
  KEEP_DISTANCE,
  /// Walk `far_tiles` away from where the target was seen.
  FLEE,
  /// Keep within `near_tiles` of the target: pursue, but for a friend.
  FOLLOW,
  /// Walk to where the target was last seen, and stand there.
  SEARCH,
  /// Walk back to where the actor spawned.
  RETURN_HOME,
  /// Run straight ahead along the facing, not steering, until something
  /// stops it or the state ends: a charger's rush.
  CHARGE,
};

}  // namespace eng::game
