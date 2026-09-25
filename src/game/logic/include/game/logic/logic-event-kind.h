#pragma once

/// @file logic-event-kind.h
/// @brief What happened, in an event game logic is told of.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// Everything `GameLogicWorld::events` reports.
enum class LogicEventKind : uint8_t {
  /// An actor joined the run: spawned by the logic.
  ACTOR_SPAWNED,
  /// An actor was hurt, and lived.
  ACTOR_HURT,
  /// An actor was killed — by anything: a shot, a blast, the logic.
  ACTOR_DIED,
  /// An actor was taken out of the run by the logic, without dying.
  ACTOR_REMOVED,
  /// A player was hurt, and is still up.
  PLAYER_HURT,
  /// A player went down.
  PLAYER_DOWNED,
  /// A downed player was brought back up; `by` is the teammate who did it.
  PLAYER_REVIVED,
  /// A downed player's window ran out, or nobody was left to revive them:
  /// they are out of the run.
  PLAYER_OUT,
  /// An actor went into another state of its behavior — by its own
  /// behavior, or the logic's `setActorState`. `state` is the state's id.
  ACTOR_STATE_ENTERED,
  /// An actor took someone new as its target; `other` is whom.
  ACTOR_NOTICED,
  /// An actor attacked — struck, fired, spat, or blew itself up; `other`
  /// is whom it had in mind, if anyone.
  ACTOR_ATTACKED,
};

}  // namespace eng::game
