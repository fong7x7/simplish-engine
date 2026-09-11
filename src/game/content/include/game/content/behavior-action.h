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
/// "The target" is whom the actor last perceived, and "where it was
/// seen" is where the actor last perceived them — an actor never knows
/// where anyone is without seeing or hearing them.
///
/// The attacks — `melee`, `charge`, `fire`, `spit`, `detonate` — never hurt
/// anyone where they happen: each appends what it does to the tick's
/// effects buffer, and the damage phase applies it (Game §5.3).
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
  /// stops it or the state ends: a charger's rush. Hits its target on
  /// contact, as `melee` does.
  CHARGE,
  /// Walk the route its prop names, waypoint by waypoint, as the state's
  /// `route` says — round and round, or back and forth. It carries on from
  /// where it left off when it comes back to the state; one with no route
  /// stands.
  PATROL,
  /// Close in on the target as `pursue` does, to touching, and hit it
  /// whenever it is within reach and the attack's cooldown has run.
  MELEE,
  /// Stand and fire a volley of projectiles at the target it sees, fanned
  /// across the attack's spread, every cooldown.
  FIRE,
  /// Stand and lob a hazard pool where the target was seen, every
  /// cooldown: the spitter of Game §5.1.
  SPIT,
  /// Blow up at once, hurting everyone within the attack's radius — every
  /// side — and die: the bloater of Game §5.1.
  DETONATE,
};

}  // namespace eng::game
