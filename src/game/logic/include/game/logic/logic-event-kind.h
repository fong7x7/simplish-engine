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
  /// An actor began an attack that winds up — a swing, a fuse — to land
  /// its behavior's `windup_ticks` later, if it still can; `other` is whom
  /// it has in mind.
  ACTOR_WINDING_UP,
  /// A player's foot came down: they covered their feet's stride. Heard
  /// only while the logic listens for steps.
  PLAYER_STEPPED,
  /// An actor's foot came down. Heard only while the logic listens for
  /// everyone's steps.
  ACTOR_STEPPED,
  /// A player chose an action on one of the game's screens — a button —
  /// on the tick before; `id` is the action (ADR-012).
  UI_ACTION,
  /// A player started pressing the pause button — bound to P and a pad's
  /// Start unless changed — on the tick before. Heard paused or not: the
  /// logic decides what it does (`sdk::togglePause`).
  PAUSE_PRESSED,
};

/// How many kinds of event there are: whatever hands each kind to its own
/// handler checks it has one for every kind against this.
inline constexpr uint8_t LOGIC_EVENT_KIND_COUNT =
    static_cast<uint8_t>(LogicEventKind::PAUSE_PRESSED) + 1;

}  // namespace eng::game
